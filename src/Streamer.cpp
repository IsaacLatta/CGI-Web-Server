#include "Streamer.h"
#include "http/Exception.h"

asio::awaitable<void> StringStreamer::stream(io::Socket* sock) {
    std::span<const char> buffer(payload->data(), payload->length());
    auto result = co_await http::co_write_all(sock, buffer);
    if(!http::is_success_code(result.status)) {
        throw http::Exception(result.status, std::move(result.message));
    }
    bytes_streamed = result.bytes;
    co_return;
}

FileStreamer::~FileStreamer() {
    if(filefd_ != -1) {
        close(filefd_);
    }
}

void FileStreamer::openFile() {
    filefd_ = open(file_path.c_str(), O_RDONLY);
    if(filefd_ == -1) {
        throw http::Exception(http::Code::Not_Found, 
                std::format("Failed to open resource: {}, errno={} ({})", file_path, errno, strerror(errno)));
    }

    file_len = (long)lseek(filefd_, (off_t)0, SEEK_END);
    if(file_len <= 0) {
        throw http::Exception(http::Code::Not_Found, 
                std::format("Failed to open endpoint={}, errno={} ({})", file_path, errno, strerror(errno)));
    }
    lseek(filefd_, 0, SEEK_SET);
}

asio::awaitable<void> FileStreamer::stream(io::Socket* sock) {
    if(filefd_ == -1) {
        openFile();
    }
    
    http::WriteStatus result;
    std::size_t bytes_sent(0);
    while (bytes_sent < file_len) {
        std::size_t bytes_to_read = std::min(buffer.size(), static_cast<std::size_t>(file_len - bytes_sent));
        ssize_t bytes_to_write = read(filefd_, buffer.data(), bytes_to_read);
        if (bytes_to_write == 0) {
            DEBUG("File Streamer", "EOF reached prematurely while sending resource[%s]", file_path.c_str());
            break;
        }
        if(bytes_to_write < 0) {
            throw http::Exception(http::Code::Internal_Server_Error, std::format("Failed reading file {}", file_path));
        }

        std::span<const char> write_buffer(buffer.data(), bytes_to_write);
        result = co_await http::co_write_all(sock, write_buffer);
        if(!http::is_success_code(result.status)) {
            throw http::Exception(result.status, std::move(result.message));
        }
        bytes_sent += result.bytes;
    }
    bytes_streamed = bytes_sent;
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

    char* argv[] = {const_cast<char*>(script_path_.c_str()), (char*)0};
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

asio::awaitable<void> ScriptStreamer::stream(io::Socket* sock) {
    Spawn();
    
    asio::posix::stream_descriptor reader(sock->GetRawSocket().get_executor(), stdout_pipe_[0]);
    asio::error_code read_ec;
    std::size_t bytes_read(0), bytes_sent(0);
    std::vector<char> buffer(io::BUFFER_SIZE);
    http::WriteStatus result;

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

        std::span<const char> write_buffer(buffer.data(), bytes_read);
        result = co_await io::co_write_all(*sock, write_buffer);
        if(!http::is_success_code(result.status)) {
            throw http::Exception(result.status, std::move(result.message));
        }
        bytes_sent += result.bytes;
    }
    waitpid(pid_, &status_, 0);
    bytes_streamed = bytes_sent;
    co_return;
}

