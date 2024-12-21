#include "MethodHandler.h"
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
    std::string resource, content_type;

    auto config = cfg::Config::getInstance();
    const cfg::Route* route = config->findRoute(request->endpoint);
    if(route && route->is_protected) {
        // authenticate the user
    }
    else {
        request->endpoint = "public/" + request->endpoint;
    }

    int filefd =  open(resource.c_str(), O_RDONLY | O_NONBLOCK);
    if(filefd == -1) {
        throw http::HTTPException(http::code::Not_Found, std::format("Failed to open resource: {}, errno={} ({})", resource.c_str(), errno, strerror(errno)));
    }
    
    long file_len;
    std::string response_header = buildHeader(filefd, content_type, file_len);
    if(file_len < 0) {
        close(filefd);
        throw http::HTTPException(http::code::Not_Found, "404 File not found: " + resource);
    }

    TransferState state;
    while (state.retry_count < TransferState::MAX_RETRIES) {
        auto [error, bytes_written] = co_await sock->co_write(response_header.data(), response_header.length());
        if (!error) {
            co_return;
        }
        state.retry_count++;
        asio::steady_timer timer = asio::steady_timer(co_await asio::this_coro::executor, TransferState::RETRY_DELAY * state.retry_count);
        co_await timer.async_wait(asio::use_awaitable);
    }
    throw http::HTTPException(http::code::Internal_Server_Error, "Max retries reached HEAD request, resource: " + resource);
}