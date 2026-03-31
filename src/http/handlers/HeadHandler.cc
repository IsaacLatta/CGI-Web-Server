#include "http/handlers/MethodHandler.h"
#include "http/Session.h"
#include "http/Exception.h"

#include "Streamer.h"

void HeadHandler::buildResponse() {
    const std::string& file = txn_.ResolvedEndpoint->ResourceName;
    int filefd =  open(file.c_str(), O_RDONLY);
    if(filefd == -1) {
        throw http::Exception(http::Not_Found,
              std::format("failed to open file={} errno={} ({})", file, errno, strerror(errno)));
    }

    const long file_len = (long)lseek(filefd, (off_t)0, SEEK_END);
    if(file_len <= 0) {
        throw http::Exception(http::Not_Found,
              std::format("failed to open file={}, errno={} ({})", file, errno, strerror(errno)));
    }
    lseek(filefd, 0, SEEK_SET);
    close(filefd);

    std::string content_type;
    if(http::determine_content_type(txn_.ResolvedEndpoint->ResourceName, content_type) != http::OK) {
        throw http::Exception(http::Forbidden, std::format("failed to extract content type from file={}", file));
    }

    txn_.GetResponse()
        .SetStatus(http::OK)
        .AddHeader("Connection", "close")
        .AddHeader("Content-Type", content_type)
        .AddHeader("Content-Length", std::to_string(file_len));
}

asio::awaitable<void> HeadHandler::Handle() {
    if (!txn_.ResolvedEndpoint || txn_.ResolvedEndpoint->HasScript) {
        throw http::Exception(http::Internal_Server_Error, "routing failure in head handler");
    }

    buildResponse();
    const std::string response_header = txn_.GetResponse().Build();
    StringStreamer stream(response_header);
    co_await stream.Stream(txn_.GetSocket());
    txn_.addBytes(stream.BytesStreamed());
    co_return;
}
