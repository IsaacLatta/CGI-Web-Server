#include "MethodHandler.h"
#include "Session.h"

void HeadHandler::buildResponse() {
    int filefd =  open(request->endpoint_url.c_str(), O_RDONLY);
    if(filefd == -1) {
        throw http::HTTPException(http::code::Not_Found, 
              std::format("Failed to open resource: {}, errno={} ({})", request->endpoint_url, errno, strerror(errno)));
    }
    
    long file_len;
    file_len = (long)lseek(filefd, (off_t)0, SEEK_END);
    if(file_len <= 0) {
        throw http::HTTPException(http::code::Not_Found, 
              std::format("Failed to open endpoint={}, errno={} ({})", request->endpoint_url, errno, strerror(errno)));
    }
    lseek(filefd, 0, SEEK_SET);
    close(filefd);

    std::string content_type;
    if(http::determine_content_type(request->endpoint_url, content_type) != http::code::OK) {
        throw http::HTTPException(http::code::Forbidden, std::format("Failed to extract content_type for endpoint={}", request->endpoint_url));
    }

    response->setStatus(http::code::OK);
    response->addHeader("Connection", "close");
    response->addHeader("Content-Type", content_type);
    response->addHeader("Content-Length", std::to_string(file_len));
}

asio::awaitable<void> HeadHandler::handle() {    
    if(!request->route || !request->route->isMethodProtected(request->method)) {
        request->endpoint_url = "public/" + request->endpoint_url;
    }
    
    buildResponse();
    txn->finish = [self = shared_from_this(), txn = this->txn]() ->asio::awaitable<void> {
        std::string response_header = txn->getResponse()->build();
        auto sock = txn->getSocket();
        TransferState state;
        asio::error_code error;
        while(state.retry_count < TransferState::MAX_RETRIES) {
            auto [error, bytes_written] = co_await sock->co_write(response_header.data(), response_header.length());
            
            if (!error) {
                co_return;
            }

            state.retry_count++;
            asio::steady_timer timer = asio::steady_timer(co_await asio::this_coro::executor, TransferState::RETRY_DELAY * state.retry_count);
            co_await timer.async_wait(asio::use_awaitable);
        }
        throw http::HTTPException(http::code::Internal_Server_Error, std::format("MAX_RETRIES reached sending {} to {} with header {}\nERROR INFO: error={} ({})", 
            txn->request.endpoint_url, sock->getIP(), response_header, error.value(), error.message()));
    };
    co_return;
}
