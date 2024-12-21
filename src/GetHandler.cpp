#include "MethodHandler.h"
#include "Session.h"


asio::awaitable<void> GetHandler::sendResource(int filefd, long file_len) {
    asio::posix::stream_descriptor file_desc(co_await asio::this_coro::executor, filefd);
    
    std::vector<char> buffer(BUFFER_SIZE);
    TransferState state{.total_bytes = file_len};
    while (state.bytes_sent < file_len) {
        std::size_t bytes_to_read = std::min(buffer.size(), static_cast<std::size_t>(file_len - state.bytes_sent));
        ssize_t bytes_to_write = ::read(filefd, buffer.data(), bytes_to_read);
        if (bytes_to_write == 0) {
            LOG("WARN", "Get Handler", "EOF reached prematurely while sending resource");
            break;
        }
        if(bytes_to_write < 0) {
            close(filefd);
            throw http::HTTPException(http::code::Internal_Server_Error, "Failed reading file");
        }

        state.retry_count = 1;
        std::size_t total_bytes_written = 0;
        while (total_bytes_written < bytes_to_write) {
            auto [ec, bytes_written] = co_await sock->co_write(buffer.data() + total_bytes_written, bytes_to_write - total_bytes_written);
            if (ec == asio::error::connection_reset || ec == asio::error::broken_pipe || ec == asio::error::eof) {
                close(filefd);
                throw http::HTTPException(http::code::Client_Closed_Request, "Connection reset by client");
            }

            if (ec && state.retry_count <= TransferState::MAX_RETRIES) {
                LOG("WARN", "Get Handler", "Retry %d of %d for sending resource", state.retry_count, TransferState::MAX_RETRIES);
                state.retry_count++;

                asio::steady_timer timer(co_await asio::this_coro::executor);
                timer.expires_after(asio::chrono::milliseconds(TransferState::RETRY_DELAY * state.retry_count));
                co_await timer.async_wait(asio::use_awaitable);
                continue;
            }
            total_bytes_written += bytes_written;
            state.retry_count = 0;
        }

        state.bytes_sent += total_bytes_written;
    }
    
    close(filefd);
    LOG("INFO", "Get Handler", "Finished sending file, file size: %ld, bytes sent: %ld", file_len, state.bytes_sent);
}

asio::awaitable<void> GetHandler::handle() {    
    int filefd =  open(request->endpoint.c_str(), O_RDONLY);
    if(filefd == -1) {
        throw http::HTTPException(http::code::Not_Found, 
              std::format("Failed to open resource: {}, errno={} ({})", request->endpoint, errno, strerror(errno)));
    }
    
    long file_len;
    file_len = (long)lseek(filefd, (off_t)0, SEEK_END);
    if(file_len <= 0) {
        throw http::HTTPException(http::code::Not_Found, 
              std::format("Failed to open endpoint={}, errno={} ({})", request->endpoint, errno, strerror(errno)));
    }
    lseek(filefd, 0, SEEK_SET);

    std::string content_type;
    if(http::determine_content_type(request->endpoint, content_type) != http::code::OK) {
        throw http::HTTPException(http::code::Forbidden, std::format("Failed to extract content_type for endpoint={}", request->endpoint));
    }

    response->setStatus(http::code::OK);
    response->addHeader("Connection", "close");
    response->addHeader("Content-Type", content_type);
    response->addHeader("Content-Length", std::to_string(file_len));
    std::string response_header = response->build();

    LOG("DEBUG", "GetHandler", "RESPONSE HEADER: %s", response_header.data());

    asio::error_code error;
    TransferState state;
    while(state.retry_count < TransferState::MAX_RETRIES) {
        auto [error, bytes_written] = co_await sock->co_write(response_header.data(), response_header.length());
        
        if (!error) {
            co_await sendResource(filefd, file_len);
            co_return;
        }

        state.retry_count++;
        asio::steady_timer timer = asio::steady_timer(co_await asio::this_coro::executor, TransferState::RETRY_DELAY * state.retry_count);
        co_await timer.async_wait(asio::use_awaitable);
    }
    throw http::HTTPException(http::code::Internal_Server_Error, std::format("MAX_RETRIES reached sending {} to {} with header {}\nERROR INFO: error={} ({})", 
          request->endpoint, sock->getIP(), response_header, error.value(), error.message()));
}
