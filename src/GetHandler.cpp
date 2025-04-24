#include "MethodHandler.h"
#include "Session.h"
#include "Streamer.h"

asio::awaitable<void> GetHandler::handleScript() {
    bool first_read = true;
    auto chunk_callback = [first_read, response = txn->getResponse(), sock = txn->getSocket()](const char* buf, std::size_t len) -> asio::awaitable<void> {
        if(!first_read) {
            co_return;
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
        std::span<const char> header_buf(response_str.data(), response_str.length());
        http::io::WriteStatus result = co_await http::io::co_write_all(sock, header_buf);
        if(!http::is_success_code(result.status)) {
            throw http::HTTPException(result.status, std::move(result.message));
        }
        co_return;
    };

    std::string script = request->route->getScript(request->method);
    std::string args(request->args);
    ScriptStreamer streamer(script, args, chunk_callback);
    co_await streamer.stream(txn->getSocket());
    txn->addBytes(streamer.getBytesStreamed());
    co_return;
}

asio::awaitable<void> GetHandler::handleFile() {
    if(!request->route->isMethodProtected(request->method)) {
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
    response->addHeader("Content-Type", std::to_string(f_stream.getFileSize()));
    co_await f_stream.stream(txn->getSocket());
    txn->addBytes(f_stream.getBytesStreamed() + s_stream.getBytesStreamed());
    co_return;
}

asio::awaitable<void> GetHandler::handle() {    
    if(!request->route) {
        throw http::HTTPException(http::code::Service_Unavailable, 
        std::format("No GET route found for endpoint={}", request->endpoint_url));
    }
    
    auto route = request->route;
    std::string script =  route->getScript(request->method);
    if(!script.empty()) {
        co_await handleScript();
        co_return;
    } 
    co_await handleFile();
    co_return;    
}
