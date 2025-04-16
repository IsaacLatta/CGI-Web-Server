#include "MethodHandler.h"
#include "Session.h"

asio::awaitable<void> PostHandler::handle() {
    if(request->route == nullptr) {
        throw http::HTTPException(http::code::Method_Not_Allowed, std::format("No POST route found for endpoint: {}", request->endpoint_url));
    }
    
    http::json args;
    http::code code;
    if((code = http::build_json(*(txn->getBuffer()), args)) != http::code::OK) {
        throw http::HTTPException(code, "Failed to build json array");
    }

    auto buffer_callback = [response = txn->getResponse(), sock = txn->getSocket()](const char* buf, std::size_t len){
        std::vector<char> buffer(buf, buf + len); // will need to replace this later
        response->status_msg = http::extract_header_line(buffer);
        if((response->status = http::extract_status_code(buffer)) == http::code::Bad_Request) {
            throw http::HTTPException(http::code::Bad_Gateway, 
            std::format("Failed to parse response from script"));
        }

        response->body = http::extract_body(buffer);
        response->headers = http::extract_headers(buffer);
        response->addHeader("Connection", "close");
        std::string response_str = response->build();
        sock->write(response_str.data(), response_str.length());
    };

    std::string args_str = "";
    try {
        args_str = args.dump();
    } catch(const std::exception& e) {
        throw http::HTTPException(http::code::Bad_Request, 
        std::format("failed to create json array: {}", e.what())); // assume failure to parse client's args is a bad request, let error handler catch the http error
    }
    std::string script = request->route->getScript(request->method);
    ScriptStreamer streamer(script, args_str, buffer_callback);
    co_await streamer.prepare(txn->getResponse()); 
    co_await streamer.stream(txn->getSocket());
    
    co_return; 
}

