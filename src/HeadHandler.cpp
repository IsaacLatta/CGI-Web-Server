#include "MethodHandler.h"
#include "Session.h"

void HeadHandler::buildResponse() {
    std::string file = request->route->getResource(request->method);
    int filefd =  open(file.c_str(), O_RDONLY);
    if(filefd == -1) {
        throw http::HTTPException(http::code::Not_Found, 
              std::format("failed to open file={}, for endpoint={}: errno={} ({})", file, request->endpoint_url, errno, strerror(errno)));
    }
    
    long file_len;
    file_len = (long)lseek(filefd, (off_t)0, SEEK_END);
    if(file_len <= 0) {
        throw http::HTTPException(http::code::Not_Found, 
              std::format("failed to open file={}, for endpoint={}: errno={} ({})", file, request->endpoint_url, errno, strerror(errno)));
    }
    lseek(filefd, 0, SEEK_SET);
    close(filefd);

    std::string content_type;
    if(http::determine_content_type(request->route->getResource(request->method), content_type) != http::code::OK) {
        throw http::HTTPException(http::code::Forbidden, std::format("failed to extract content type for endpoint={}, from file={}", request->endpoint_url, file));
    }

    response->setStatus(http::code::OK);
    response->addHeader("Connection", "close");
    response->addHeader("Content-Type", content_type);
    response->addHeader("Content-Length", std::to_string(file_len));
}

asio::awaitable<void> HeadHandler::handle() {    
    if(!request->route) {
        throw http::HTTPException(http::code::Service_Unavailable, 
        std::format("No GET route found for endpoint={}", request->endpoint_url));
    }
    buildResponse();
    std::string response_header = txn->getResponse()->build();
    StringStreamer stream(&response_header);
    co_await stream.stream(txn->getSocket());
    txn->addBytes(stream.getBytesStreamed());
    co_return;
}
