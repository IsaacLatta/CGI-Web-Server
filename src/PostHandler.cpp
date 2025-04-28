#include "MethodHandler.h"
#include "Session.h"

asio::awaitable<void> PostHandler::handle() {
    if(request->endpoint == nullptr || !request->route || !request->route->has_script) {
        throw http::HTTPException(http::code::Service_Unavailable, 
        std::format("No POST route found for endpoint={}", request->endpoint_url));
    }

    bool first_read = true;
    auto chunk_callback = [first_read, response = txn->getResponse(), sock = txn->getSocket()] (const char* buf, std::size_t len) -> asio::awaitable<void> {
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
        auto headers = http::extract_headers(buffer);
        response->addHeaders(headers);
        response->addHeader("Connection", "close");
        std::string response_str = response->build();

        std::span<const char> header_buf(response_str.data(), response_str.length());
        http::io::WriteStatus result = co_await http::io::co_write_all(sock, header_buf);
        if(!http::is_success_code(result.status)) {
            throw http::HTTPException(result.status, std::move(result.message));
        }
        co_return;
    };

    std::string script = request->route->resource;
    std::string args(request->args);
    ScriptStreamer streamer(script, args, chunk_callback);
    co_await streamer.stream(txn->getSocket());
    txn->addBytes(streamer.getBytesStreamed());
    co_return; 
}

