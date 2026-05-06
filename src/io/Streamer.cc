#include "io/Streamer.h"
#include "io/Acceptor.h"
#include "io/Socket.h"

#include <spawn.h>
#include <sys/wait.h>

asio::awaitable<io::Result> io::StringStreamer::Stream(io::Socket& sock) {
    const auto result = co_await io::co_write_all(sock, payload_);
    if (result.ec) {
        co_return Result { result.ec, result.bytes };
    }

    bytes_streamed_ = result.bytes;
    co_return Result { {}, result.bytes };
}

io::FileStreamer::~FileStreamer() {
    if (filefd_ != -1) {
        close(filefd_);
    }
}

io::Result io::FileStreamer::OpenStream() {
    filefd_ = open(file_path_.c_str(), O_RDONLY);
    if (filefd_ == -1) {
        const auto ec = std::error_code(errno, std::generic_category());
        ERROR("FileStreamer", "Failed to open resource=%s, errno=%s", file_path_.c_str(), ec.message().c_str());
        return Result { ec, 0u };
    }

    const off_t len = lseek(filefd_, 0, SEEK_END);
    if (len < 0) {
        const auto ec = std::error_code(errno, std::generic_category());
        ERROR("FileStreamer", "Failed to seek resource=%s, errno=%s", file_path_.c_str(), ec.message().c_str());
        close(filefd_);
        filefd_ = -1;
        return Result { ec, 0u };
    }

    file_len_ = static_cast<long>(len);

    if (lseek(filefd_, 0, SEEK_SET) < 0) {
        const auto ec = std::error_code(errno, std::generic_category());
        ERROR("FileStreamer", "Failed to rewind resource=%s, errno=%s", file_path_.c_str(), ec.message().c_str());
        close(filefd_);
        filefd_ = -1;
        return Result { ec, 0u };
    }

    return Result { {}, 0u };
}

asio::awaitable<io::Result> io::FileStreamer::Stream(io::Socket& sock) {
    if (filefd_ == -1) {
        io::Result open_result = OpenStream();
        if (open_result.ec) {
            co_return open_result;
        }
    }

    size_t bytes_sent { 0u };

    while (bytes_sent < static_cast<size_t>(file_len_)) {
        const size_t bytes_to_read = std::min(buffer_.size(), static_cast<size_t>(file_len_) - bytes_sent);

        const ssize_t bytes_read = read(filefd_, buffer_.data(), bytes_to_read);
        if (bytes_read == 0) {
            DEBUG("FileStreamer", "EOF reached prematurely while sending resource=%s", file_path_.c_str());
            break;
        }

        if (bytes_read < 0) {
            const auto ec = std::error_code(errno, std::generic_category());
            ERROR("FileStreamer", "Failed reading resource=%s, errno=%s", file_path_.c_str(), ec.message().c_str());
            co_return Result { ec, bytes_sent };
        }

        const std::span<const char> write_buffer(buffer_.data(), static_cast<size_t>(bytes_read));
        const io::Result result = co_await io::co_write_all(sock, write_buffer);

        if (result.ec) {
            co_return Result { result.ec, bytes_sent };
        }

        bytes_sent += result.bytes;
    }

    bytes_streamed_ = bytes_sent;
    co_return Result { {}, bytes_sent };
}


io::ScriptStreamer::~ScriptStreamer() {
    (void)close(stdin_pipe_[0]);
    (void)close(stdin_pipe_[1]);
    (void)close(stdout_pipe_[0]);
    (void)close(stdout_pipe_[1]);
}

extern char** environ;

io::Result io::ScriptStreamer::Spawn() {
    if (pipe(stdin_pipe_) < 0) {
        const auto ec = std::error_code(errno, std::generic_category());
        ERROR("io::ScriptStreamer", "Failed to create stdin pipe for script=%s, errno=%s", script_path_.c_str(), ec.message().c_str());
        return Result { ec, 0u };
    }

    if (pipe(stdout_pipe_) < 0) {
        const auto ec = std::error_code(errno, std::generic_category());
        ERROR("io::ScriptStreamer", "Failed to create stdout pipe for script=%s, errno=%s", script_path_.c_str(), ec.message().c_str());
        close(stdin_pipe_[0]);
        close(stdin_pipe_[1]);
        return Result { ec, 0u };
    }

    auto result = SpawnProcess();
    if (result.ec) {
        close(stdin_pipe_[0]);
        close(stdin_pipe_[1]);
        close(stdout_pipe_[0]);
        close(stdout_pipe_[1]);
        return result;
    }

    const char* data = stdin_data_.data();
    std::size_t remaining = stdin_data_.size();

    while (remaining > 0) {
        const ssize_t bytes = write(stdin_pipe_[1], data, remaining);
        if (bytes < 0) {
            if (errno == EINTR) {
                continue;
            }
            const auto ec = std::error_code(errno, std::generic_category());
            ERROR("io::ScriptStreamer", "Failed to write stdin to script=%s pid=%d, errno=%s", script_path_.c_str(), pid_, ec.message().c_str());
            close(stdin_pipe_[1]);
            return Result { ec, 0u };
        }

        if (bytes == 0) {
            const auto ec = std::make_error_code(std::errc::io_error);
            ERROR("io::ScriptStreamer", "Failed to write stdin to script=%s pid=%d, write returned 0", script_path_.c_str(), pid_);
            close(stdin_pipe_[1]);
            return Result { ec, 0u };
        }

        data += bytes;
        remaining -= static_cast<size_t>(bytes);
    }

    if (close(stdin_pipe_[1]) < 0) {
        const auto ec = std::error_code(errno, std::generic_category());
        ERROR("io::ScriptStreamer", "Failed to close stdin pipe for script=%s pid=%d, errno=%s", script_path_.c_str(), pid_, ec.message().c_str());
        return Result { ec, 0u };
    }

    return Result { {}, 0u };
}

io::Result io::ScriptStreamer::SpawnProcess() {
    posix_spawn_file_actions_t actions;

    int rc = posix_spawn_file_actions_init(&actions);
    if (rc != 0) {
        const auto ec = std::error_code(rc, std::generic_category());
        ERROR("io::ScriptStreamer", "failed to init file actions for script=%s, error=%s", script_path_.c_str(), ec.message().c_str());
        return Result { ec, 0u };
    }

    rc = posix_spawn_file_actions_adddup2(&actions, stdin_pipe_[0], STDIN_FILENO);
    if (rc != 0) {
        const auto ec = std::error_code(rc, std::generic_category());
        ERROR("io::ScriptStreamer", "failed to add stdin dup2 action for script=%s, error=%s", script_path_.c_str(), ec.message().c_str());
        posix_spawn_file_actions_destroy(&actions);
        return Result { ec, 0u };
    }

    rc = posix_spawn_file_actions_adddup2(&actions, stdout_pipe_[1], STDOUT_FILENO);
    if (rc != 0) {
        const auto ec = std::error_code(rc, std::generic_category());
        ERROR("io::ScriptStreamer", "failed to add stdout dup2 action for script=%s, error=%s", script_path_.c_str(), ec.message().c_str());
        posix_spawn_file_actions_destroy(&actions);
        return Result { ec, 0u };
    }

    rc = posix_spawn_file_actions_addclose(&actions, stdin_pipe_[1]);
    if (rc != 0) {
        const auto ec = std::error_code(rc, std::generic_category());
        ERROR("io::ScriptStreamer", "failed to add stdin close action for script=%s, error=%s", script_path_.c_str(), ec.message().c_str());
        posix_spawn_file_actions_destroy(&actions);
        return Result { ec, 0u };
    }

    rc = posix_spawn_file_actions_addclose(&actions, stdout_pipe_[0]);
    if (rc != 0) {
        const auto ec = std::error_code(rc, std::generic_category());
        ERROR("io::ScriptStreamer", "failed to add stdout close action for script=%s, error=%s", script_path_.c_str(), ec.message().c_str());
        posix_spawn_file_actions_destroy(&actions);
        return Result { ec, 0u };
    }

    char* argv[] = {
        const_cast<char*>(script_path_.c_str()),
        nullptr
    };

    rc = posix_spawn(&pid_, script_path_.c_str(), &actions, nullptr, argv, environ);

    int destroy_rc = posix_spawn_file_actions_destroy(&actions);
    if (destroy_rc != 0) {
        const auto ec = std::error_code(destroy_rc, std::generic_category());
        ERROR("io::ScriptStreamer", "failed to destroy file actions for script=%s, error=%s", script_path_.c_str(), ec.message().c_str());
        return Result { ec, 0u };
    }

    if (rc != 0) {
        const auto ec = std::error_code(rc, std::generic_category());
        ERROR("io::ScriptStreamer", "failed to launch script=%s with posix_spawn, error=%s", script_path_.c_str(), ec.message().c_str());
        return Result { ec, 0u };
    }

    if (close(stdin_pipe_[0]) < 0) {
        const auto ec = std::error_code(errno, std::generic_category());
        ERROR("io::ScriptStreamer", "failed to close parent stdin read pipe for script=%s pid=%d, error=%s", script_path_.c_str(), pid_, ec.message().c_str());
        return Result { ec, 0u };
    }

    if (close(stdout_pipe_[1]) < 0) {
        const auto ec = std::error_code(errno, std::generic_category());
        ERROR("io::ScriptStreamer", "failed to close parent stdout write pipe for script=%s pid=%d, error=%s", script_path_.c_str(), pid_, ec.message().c_str());
        return Result { ec, 0u };
    }

    return Result { {}, 0u };
}

io::Result io::ScriptStreamer::OpenStream() {
    return SpawnProcess();
}

asio::awaitable<io::Result> io::ScriptStreamer::Stream(Socket& sock) {
    io::Result spawn_result = Spawn();
    if (spawn_result.ec) {
        co_return spawn_result;
    }

    asio::posix::stream_descriptor reader(sock.GetRawSocket().get_executor(), stdout_pipe_[0]);

    asio::error_code read_ec;
    size_t bytes_read { 0u };
    size_t bytes_sent { 0u };
    std::vector<char> buffer(io::BUFFER_SIZE);

    while (true) {
        std::tie(read_ec, bytes_read) = co_await reader.async_read_some(
            asio::buffer(buffer.data(), buffer.size()),
            asio::as_tuple(asio::use_awaitable)
        );

        if (read_ec == asio::error::eof) {
            break;
        }

        if (read_ec) {
            ERROR("io::ScriptStreamer", "Failed to read response from subprocess=%s pid=%d, asio_error=%d (%s)",
                script_path_.c_str(), pid_, read_ec.value(), read_ec.message().c_str());
            co_return Result { read_ec, bytes_sent };
        }

        if (bytes_read == 0) {
            continue;
        }

        if (chunk_callback_) {
            io::Result callback_result = co_await chunk_callback_(buffer.data(), bytes_read);
            if (callback_result.ec) {
                co_return Result { callback_result.ec, bytes_sent };
            }

            bytes_sent += callback_result.bytes;
            continue;
        }

        const std::span<const char> write_buffer(buffer.data(), bytes_read);
        const io::Result write_result = co_await io::co_write_all(sock, write_buffer);

        if (write_result.ec) {
            co_return Result { write_result.ec, bytes_sent };
        }

        bytes_sent += write_result.bytes;
    }

    if (waitpid(pid_, &status_, 0) < 0) {
        const auto ec = std::error_code(errno, std::generic_category());
        ERROR("io::ScriptStreamer", "waitpid failed for subprocess=%s pid=%d, error=%s",  script_path_.c_str(), pid_, ec.message().c_str());
        co_return Result { ec, bytes_sent };
    }

    bytes_streamed_ = bytes_sent;
    co_return Result { {}, bytes_sent };
}

