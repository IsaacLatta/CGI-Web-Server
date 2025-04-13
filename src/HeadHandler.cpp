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
    std::string response_header = txn->getResponse()->build();
    StringStreamer stream(&response_header);
    co_await stream.stream(txn->getSocket());
    txn->addBytes(stream.getBytesStreamed());
    co_return;
}
