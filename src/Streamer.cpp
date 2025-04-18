#include "Streamer.h"

static asio::awaitable<bool> check_error(const asio::error_code& ec, TransferState& state) {
    if (ec == asio::error::connection_reset || ec == asio::error::broken_pipe || ec == asio::error::eof) {
        throw http::HTTPException(http::code::Client_Closed_Request, "Connection reset by client");
    }

    if(ec && state.retry_count > TransferState::MAX_RETRIES) {
        throw http::HTTPException(http::code::Internal_Server_Error, "Max retries reached while streaming");
    }

    if (ec && state.retry_count <= TransferState::MAX_RETRIES) {
        state.retry_count++;
        asio::steady_timer timer(co_await asio::this_coro::executor);
        timer.expires_after(asio::chrono::milliseconds(TransferState::RETRY_DELAY * state.retry_count));
        co_await timer.async_wait(asio::use_awaitable);
        co_return true;
    }
    co_return false;
}

asio::awaitable<void> StringStreamer::stream(Socket* sock) {
    TransferState state;
    std::size_t payload_size = payload->length();
    while(state.bytes_sent < payload_size) {
        auto [ec, bytes_written] = co_await sock->co_write(payload->data() + state.bytes_sent, payload_size - state.bytes_sent);
        
        if(co_await check_error(ec, state)) {
            continue;
        }

        state.bytes_sent += bytes_written;
    }
    if(state.bytes_sent != payload_size) {
        throw http::HTTPException(http::code::Internal_Server_Error, std::format("failed to send payload {}/{} bytes sent", state.bytes_sent, payload_size));
    }

    this->bytes_streamed = state.bytes_sent;
    co_return;
}

FileStreamer::~FileStreamer() {
    if(filefd != -1) {
        close(filefd);
    }
}

void FileStreamer::openFile() {
    filefd = open(file_path.c_str(), O_RDONLY);
    if(filefd == -1) {
        throw http::HTTPException(http::code::Not_Found, 
                std::format("Failed to open resource: {}, errno={} ({})", file_path, errno, strerror(errno)));
    }

    file_len = (long)lseek(filefd, (off_t)0, SEEK_END);
    if(file_len <= 0) {
        throw http::HTTPException(http::code::Not_Found, 
                std::format("Failed to open endpoint={}, errno={} ({})", file_path, errno, strerror(errno)));
    }
    lseek(filefd, 0, SEEK_SET);
}

asio::awaitable<void> FileStreamer::stream(Socket* sock) {
    if(filefd == -1) {
        openFile();
    }
    
    TransferState state{.total_bytes = file_len};
    while (state.bytes_sent < file_len) {
        std::size_t bytes_to_read = std::min(buffer.size(), static_cast<std::size_t>(file_len - state.bytes_sent));
        ssize_t bytes_to_write = read(filefd, buffer.data(), bytes_to_read);
        if (bytes_to_write == 0) {
            DEBUG("File Streamer", "EOF reached prematurely while sending resource[%s]", file_path.c_str());
            break;
        }
        if(bytes_to_write < 0) {
            throw http::HTTPException(http::code::Internal_Server_Error, std::format("Failed reading file {}", file_path));
        }

        state.retry_count = 1;  
        std::size_t total_bytes_written = 0;
        while (total_bytes_written < bytes_to_write) {
            auto [ec, bytes_written] = co_await sock->co_write(buffer.data() + total_bytes_written, bytes_to_write - total_bytes_written);

            if(co_await check_error(ec, state)) {
                continue;
            }

            total_bytes_written += bytes_written;
            state.retry_count = 0;
        }

        state.bytes_sent += bytes_to_write;
    }
    bytes_streamed = state.bytes_sent;
}

ScriptStreamer::~ScriptStreamer() {
    close(stdin_pipe[0]);
    close(stdin_pipe[1]);
    close(stdout_pipe[0]);
    close(stdout_pipe[1]);
}

extern char** environ;

void ScriptStreamer::spawnProcess() {
    posix_spawn_file_actions_t actions;
    posix_spawn_file_actions_init(&actions);
    
    if (posix_spawn_file_actions_adddup2(&actions, stdin_pipe[0], STDIN_FILENO) != 0 ||
        posix_spawn_file_actions_adddup2(&actions, stdout_pipe[1], STDOUT_FILENO) != 0) {
        posix_spawn_file_actions_destroy(&actions);
        throw http::HTTPException(http::code::Internal_Server_Error, 
        std::format("failed to add file action dup2 for script={}, error={} ({})", script_path.c_str(), status, strerror(status)));
    }

    if (posix_spawn_file_actions_addclose(&actions, stdin_pipe[1]) != 0 ||
        posix_spawn_file_actions_addclose(&actions, stdout_pipe[0]) != 0) {
        posix_spawn_file_actions_destroy(&actions);
        throw http::HTTPException(http::code::Internal_Server_Error, 
        std::format("failed to add file action close for script={}, error={} ({})", script_path.c_str(), status, strerror(status)));
    }

    char* argv[] = {const_cast<char*>(script_path.c_str()), (char*)0};
    if((status = posix_spawn(&pid, script_path.c_str(), &actions, nullptr, argv, environ)) != 0) {
        posix_spawn_file_actions_destroy(&actions);
        throw http::HTTPException(http::code::Internal_Server_Error, 
        std::format("failed to launch script={} with posix spawn, error={} ({})", script_path.c_str(), status, strerror(status)));
    }
    posix_spawn_file_actions_destroy(&actions);
    close(stdin_pipe[0]);
    close(stdout_pipe[1]);
}

void ScriptStreamer::spawn() {
    if(pipe(stdin_pipe) < 0 || pipe(stdout_pipe) < 0) {
        throw http::HTTPException(http::code::Internal_Server_Error, 
        std::format("Failed to create pipes for script={}, errno={} ({})", script_path, errno, strerror(errno)));  
    }

    spawnProcess();

    ssize_t bytes;
    if ((bytes = write(stdin_pipe[1], stdin_data.c_str(), stdin_data.length())) < 0) {
        throw http::HTTPException(http::code::Internal_Server_Error, 
        std::format("Failed to write args to script {} (pid={}), errno={}, ({})", 
        script_path.c_str(), pid, errno, strerror(errno)));
    }
    close(stdin_pipe[1]); // signal eof to child
}

asio::awaitable<void> ScriptStreamer::stream(Socket* sock) {
    spawn();
    
    asio::posix::stream_descriptor reader(sock->getRawSocket().get_executor(), stdout_pipe[0]);
    asio::error_code read_ec;
    std::size_t bytes_read;
    std::vector<char> buffer(BUFFER_SIZE);
    TransferState state;
    
    while(true) {
        std::tie(read_ec, bytes_read) = co_await reader.async_read_some(asio::buffer(buffer.data(), buffer.size()), asio::as_tuple(asio::use_awaitable));
        if(read_ec == asio::error::eof) {
            break;
        } 
        if(read_ec && read_ec != asio::error::eof) {
            throw http::HTTPException(http::code::Internal_Server_Error, 
            std::format("Failed to read response from subprocess={}, pid={}, asio::error={}, ({})", 
            script_path, pid, read_ec.value(), read_ec.message()));
        }
        if(chunk_callback) {
            chunk_callback(buffer.data(), bytes_read);
            state.bytes_sent += bytes_read;
            continue;
        }

        state.retry_count = 1;  
        std::size_t total_bytes_written = 0;
        while (total_bytes_written < bytes_read) {
            auto [ec, bytes_written] = co_await sock->co_write(buffer.data() + total_bytes_written, bytes_read - total_bytes_written);

            if(co_await check_error(ec, state)) {
                continue;
            }

            total_bytes_written += bytes_written;
            state.retry_count = 0;
        }
        state.bytes_sent += total_bytes_written;
    }
    waitpid(pid, &status, 0);
    bytes_streamed = state.bytes_sent;
    co_return;
}

