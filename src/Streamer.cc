#include "Streamer.h"
#include "http/Exception.h"

asio::awaitable<void> StringStreamer::Stream(io::Socket& sock) {
    std::span<const char> buffer(payload_);
    const auto result = co_await io::co_write_all(sock, payload_);
    if (result.ec) {
        throw http::Exception(http::error_to_status(result.ec));
    }
    bytes_streamed_ = result.bytes;
    co_return;
}

FileStreamer::~FileStreamer() {
    if(filefd_ != -1) {
        close(filefd_);
    }
}

void FileStreamer::openFile() {
    filefd_ = open(file_path_.c_str(), O_RDONLY);
    if(filefd_ == -1) {
        throw http::Exception(http::Code::Not_Found, 
                std::format("Failed to open resource: {}, errno={} ({})", file_path_, errno, strerror(errno)));
    }

    file_len_ = (long)lseek(filefd_, (off_t)0, SEEK_END);
    if(file_len_ <= 0) {
        throw http::Exception(http::Code::Not_Found, 
                std::format("Failed to open endpoint={}, errno={} ({})", file_path_, errno, strerror(errno)));
    }
    lseek(filefd_, 0, SEEK_SET);
}

asio::awaitable<void> FileStreamer::Stream(io::Socket& sock) {
    if(filefd_ == -1) {
        openFile();
    }
    
    std::size_t bytes_sent(0);
    while (bytes_sent < file_len_) {
        std::size_t bytes_to_read = std::min(buffer_.size(), static_cast<std::size_t>(file_len_ - bytes_sent));
        ssize_t bytes_to_write = read(filefd_, buffer_.data(), bytes_to_read);
        if (bytes_to_write == 0) {
            DEBUG("File Streamer", "EOF reached prematurely while sending resource[%s]", file_path_.c_str());
            break;
        }
        if(bytes_to_write < 0) {
            throw http::Exception(http::Code::Internal_Server_Error, std::format("Failed reading file {}", file_path_));
        }

        std::span<const char> write_buffer(buffer_.data(), bytes_to_write);
        const io::Socket::Result result = co_await io::co_write_all(sock, write_buffer);
        if (result.ec) {
            throw http::Exception(http::error_to_status(result.ec));
        }
        bytes_sent += result.bytes;
    }
    bytes_streamed_ = bytes_sent;
}

ScriptStreamer::~ScriptStreamer() {
    close(stdin_pipe_[0]);
    close(stdin_pipe_[1]);
    close(stdout_pipe_[0]);
    close(stdout_pipe_[1]);
}

extern char** environ;

void ScriptStreamer::SpawnProcess() {
    posix_spawn_file_actions_t actions;
    posix_spawn_file_actions_init(&actions);
    
    if (posix_spawn_file_actions_adddup2(&actions, stdin_pipe_[0], STDIN_FILENO) != 0 ||
        posix_spawn_file_actions_adddup2(&actions, stdout_pipe_[1], STDOUT_FILENO) != 0) {
        posix_spawn_file_actions_destroy(&actions);
        throw http::Exception(http::Code::Internal_Server_Error, 
        std::format("failed to add file action dup2 for script={}, error={} ({})", script_path_.c_str(), status_, strerror(status_)));
    }

    if (posix_spawn_file_actions_addclose(&actions, stdin_pipe_[1]) != 0 ||
        posix_spawn_file_actions_addclose(&actions, stdout_pipe_[0]) != 0) {
        posix_spawn_file_actions_destroy(&actions);
        throw http::Exception(http::Code::Internal_Server_Error, 
        std::format("failed to add file action close for script={}, error={} ({})", script_path_.c_str(), status_, strerror(status_)));
    }

    char* argv[] = {const_cast<char*>(script_path_.c_str()), static_cast<char *>(nullptr)};
    if((status_ = posix_spawn(&pid_, script_path_.c_str(), &actions, nullptr, argv, environ)) != 0) {
        posix_spawn_file_actions_destroy(&actions);
        throw http::Exception(http::Code::Internal_Server_Error, 
        std::format("failed to launch script={} with posix spawn, error={} ({})", script_path_.c_str(), status_, strerror(status_)));
    }
    posix_spawn_file_actions_destroy(&actions);
    close(stdin_pipe_[0]);
    close(stdout_pipe_[1]);
}

void ScriptStreamer::Spawn() {
    if(pipe(stdin_pipe_) < 0 || pipe(stdout_pipe_) < 0) {
        throw http::Exception(http::Code::Internal_Server_Error, 
        std::format("Failed to create pipes for script={}, errno={} ({})", script_path_, errno, strerror(errno)));
    }

    SpawnProcess();

    ssize_t bytes;
    if ((bytes = write(stdin_pipe_[1], stdin_data_.c_str(), stdin_data_.length())) < 0) {
        throw http::Exception(http::Code::Internal_Server_Error, 
        std::format("Failed to write args to script {} (pid={}), errno={}, ({})", 
        script_path_.c_str(), pid_, errno, strerror(errno)));
    }
    close(stdin_pipe_[1]); // signal eof to child
}

asio::awaitable<void> ScriptStreamer::Stream(io::Socket& sock) {
    Spawn();
    
    asio::posix::stream_descriptor reader(sock.GetRawSocket().get_executor(), stdout_pipe_[0]);
    asio::error_code read_ec;
    size_t bytes_read { 0u };
    size_t bytes_sent { 0u };
    std::vector<char> buffer(io::BUFFER_SIZE);

    while(true) {
        std::tie(read_ec, bytes_read) = co_await reader.async_read_some(asio::buffer(buffer.data(), buffer.size()), asio::as_tuple(asio::use_awaitable));
        if(read_ec == asio::error::eof) {
            bytes_sent += bytes_read;
            break;
        } 
        if(read_ec && read_ec != asio::error::eof) {
            throw http::Exception(http::Code::Internal_Server_Error, 
                std::format("Failed to read response from subprocess={}, pid={}, asio::error={}, ({})", 
                script_path_, pid_, read_ec.value(), read_ec.message()));
        }
        if(chunk_callback_) {
            co_await chunk_callback_(buffer.data(), bytes_read);
            bytes_sent += bytes_read;
            continue;
        }

        const std::span<const char> write_buffer(buffer.data(), bytes_read);
        const io::Socket::Result result = co_await io::co_write_all(sock, write_buffer);
        if (result.ec) {
            throw http::Exception(http::error_to_status(result.ec));
        }
        bytes_sent += result.bytes;
    }
    
    waitpid(pid_, &status_, 0);
    bytes_streamed_ = bytes_sent;
    co_return;
}

