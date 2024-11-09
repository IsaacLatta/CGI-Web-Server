#include "RequestHandler.h"
#include "Session.h"

std::string HeadHandler::buildHeader(int filefd, const std::string& content_type, long& file_len) {
    file_len = (long)lseek(filefd, (off_t)0, SEEK_END);
    if(file_len <= 0) {
        return "";
    }
    lseek(filefd, 0, SEEK_SET);
    
    std::string header = "HTTP/1.1 200 OK\r\nContent-Type: " + content_type + "\r\n"
                            "Content-Length: " + std::to_string(file_len) + "\r\n"
                            "Connection: close\r\n\r\n";
    return header;
}

asio::awaitable<void> HeadHandler::handle() {
    auto this_session = session.lock();
    if(!this_session) {
        ERROR("Get Handler", 0, "NULL", "session observer is null");
        co_return;
    }

    http::code code;
    http::clean_buffer(buffer);
    std::string resource, content_type;
    if((code = http::extract_endpoint(buffer, resource)) != http::code::OK || (code = http::extract_content_type(resource, content_type)) != http::code::OK) {
        this_session->onError(http::error(code, std::format("Parsing failed with http={} \nREQUEST: {}", static_cast<int>(code), buffer.data())));
        co_return;
    }

    LOG("INFO", "Get Handler", "REQUEST: %s \nPARSED RESULTS\n Resource: %s\nContent_Type: %s", buffer.data(), resource.c_str(), content_type.c_str());
    
    int filefd =  open(resource.c_str(), O_RDONLY | O_NONBLOCK);
    if(filefd == -1) {
        this_session->onError(http::error(http::code::Not_Found, std::format("Failed to open resource: {}, errno={} ({})", resource.c_str(), errno, strerror(errno))));
        co_return;
    }
    
    long file_len;
    std::string response_header = buildHeader(filefd, content_type, file_len);
    if(file_len < 0) {
        this_session->onError(http::error(http::code::Not_Found, "404 File not found: " + resource));
        close(filefd);
        co_return;
    }

    TransferState state;
    while (state.retry_count < TransferState::MAX_RETRIES) {
        auto [error, bytes_written] = co_await sock->co_write(response_header.data(), response_header.length());
        if (!error) {
            this_session->onCompletion(response_header, bytes_written);
            co_return;
        }
        state.retry_count++;
        asio::steady_timer timer = asio::steady_timer(co_await asio::this_coro::executor, TransferState::RETRY_DELAY * state.retry_count);
        co_await timer.async_wait(asio::use_awaitable);
    }
    this_session->onError(http::error(http::code::Internal_Server_Error, "Max retries reached HEAD request, resource: " + resource));
}