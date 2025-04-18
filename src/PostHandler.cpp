#include "MethodHandler.h"
#include "Session.h"

asio::awaitable<void> PostHandler::handle() {
    if(request->route == nullptr) {
        throw http::HTTPException(http::code::Service_Unavailable, 
        std::format("No POST route found for endpoint={}", request->endpoint_url));
    }

    bool first_read = true;
    auto chunk_callback = [first_read, response = txn->getResponse(), sock = txn->getSocket()](const char* buf, std::size_t len){
        if(!first_read) {
            return;
        }
        std::span<const char> buffer(buf, len);
        response->status_msg = http::extract_header_line(buffer);
        if((response->status = http::extract_status_code(buffer)) == http::code::Bad_Request || response->status == http::code::Not_A_Status) {
            throw http::HTTPException(http::code::Bad_Gateway, 
            std::format("Failed to parse response from script"));
        }

        response->body = http::extract_body(buffer);
        response->headers = http::extract_headers(buffer);
        response->addHeader("Connection", "close");
        std::string response_str = response->build();
        sock->write(response_str.data(), response_str.length());
    };

    std::string script = request->route->getScript(request->method);
    std::string args(request->args);
    ScriptStreamer streamer(script, args, chunk_callback);
    co_await streamer.stream(txn->getSocket());
    
    co_return; 
}

