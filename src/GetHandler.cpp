#include "MethodHandler.h"
#include "Session.h"
#include "Streamer.h"

asio::awaitable<void> GetHandler::handle() {    
    if(!request->route || !request->route->isMethodProtected(request->method)) {
        request->endpoint_url = "public/" + request->endpoint_url;
    }
    
    std::string content_type;
    if(http::determine_content_type(request->endpoint_url, content_type) != http::code::OK) {
        throw http::HTTPException(http::code::Forbidden, std::format("Failed to extract content_type for endpoint={}", request->endpoint_url));
    }

    response->setStatus(http::code::OK);
    response->addHeader("Connection", "close");
    response->addHeader("Content-Type", content_type);
    
    std::string response_header = response->build();
    StringStreamer s_stream(&response_header);
    co_await s_stream.stream(txn->getSocket());

    FileStreamer f_stream(request->endpoint_url);
    co_await f_stream.prepare(response);
    co_await f_stream.stream(txn->getSocket());
    txn->addBytes(f_stream.getBytesStreamed() + s_stream.getBytesStreamed());
    co_return;

}
