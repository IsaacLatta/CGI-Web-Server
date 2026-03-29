#include "http/handlers/MethodHandler.h"
#include "http/Session.h"
#include "http/Exception.h"

#include "Streamer.h"

void HeadHandler::buildResponse() {
    const auto& request = txn_.GetRequest();
    std::string file = request.route->resource;
    int filefd =  open(file.c_str(), O_RDONLY);
    if(filefd == -1) {
        throw http::HTTPException(http::Not_Found,
              std::format("failed to open file={}, for endpoint={}: errno={} ({})", file, request.endpoint_url, errno, strerror(errno)));
    }

    const long file_len = (long)lseek(filefd, (off_t)0, SEEK_END);
    if(file_len <= 0) {
        throw http::HTTPException(http::Not_Found,
              std::format("failed to open file={}, for endpoint={}: errno={} ({})", file, request.endpoint_url, errno, strerror(errno)));
    }
    lseek(filefd, 0, SEEK_SET);
    close(filefd);

    std::string content_type;
    if(http::determine_content_type(request.endpoint->getResource(request.method), content_type) != http::OK) {
        throw http::HTTPException(http::Forbidden, std::format("failed to extract content type for endpoint={}, from file={}", request.endpoint_url, file));
    }

    txn_.GetResponse()
        .SetStatus(http::OK)
        .AddHeader("Connection", "close")
        .AddHeader("Content-Type", content_type)
        .AddHeader("Content-Length", std::to_string(file_len));
}

asio::awaitable<void> HeadHandler::Handle() {
    const auto& request = txn_.GetRequest();

    if(!request.endpoint || !request.route || request.route->has_script) {
        throw http::HTTPException(http::Code::Service_Unavailable,
        std::format("No GET route found for endpoint={}", request.endpoint_url));
    }

    buildResponse();
    std::string response_header = txn_.GetResponse().Build();
    StringStreamer stream(&response_header);
    co_await stream.stream(txn_.GetSocket());
    txn_.addBytes(stream.getBytesStreamed());
    co_return;
}
