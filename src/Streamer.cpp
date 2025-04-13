#include "Streamer.h"

asio::awaitable<void> StringStreamer::prepare(http::Response* response) {
    response->addHeader("Content-Length", std::to_string(payload->length()));
    co_return;
}

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

asio::awaitable<void> FileStreamer::prepare(http::Response* response) {
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
    response->addHeader("Content-Length", std::to_string(file_len));
    co_return;
}

asio::awaitable<void> FileStreamer::stream(Socket* sock) {
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