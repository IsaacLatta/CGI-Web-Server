#include "MethodHandler.h"
#include "http/Session.h"
#include "http/Exception.h"
#include "Streamer.h"

asio::awaitable<void> PostHandler::Handle() {
    auto& request = txn_.GetRequest();
    
    if(request.endpoint == nullptr || !request.route || !request.route->has_script) {
        throw http::HTTPException(http::Service_Unavailable, 
        std::format("No POST route found for endpoint={}", request.endpoint_url));
    }

    bool first_read = true;
    auto chunk_callback =
        [first_read, response = txn_.GetResponse(), sock = txn_.GetSocket()] (const char* buf, std::size_t len) mutable -> asio::awaitable<void> {
            if(!first_read) {
                co_return;
            }
            std::span<const char> buffer(buf, len);
            if((response.Status = http::extract_status_code(buffer)) == http::Bad_Request || response.Status == http::Not_A_Status) {
                throw http::HTTPException(http::Bad_Gateway,
                std::format("Failed to parse response from script"));
            }

            std::string response_str = response.SetBody(std::string(http::extract_body(buffer)))
                    .AddHeaders(http::extract_headers(buffer))
                    .AddHeader("Connection", "close")
                    .Build();

            std::span<const char> header_buf(response_str.data(), response_str.length());
            http::WriteStatus result = co_await http::co_write_all(sock, header_buf);
            if(!http::is_success_code(result.status)) {
                throw http::HTTPException(result.status, std::move(result.message));
            }
            co_return;
        };

    std::string script = request.route->resource;
    std::string args(request.args);
    ScriptStreamer streamer(script, args, chunk_callback);
    co_await streamer.stream(txn_.GetSocket());
    txn_.addBytes(streamer.getBytesStreamed());
    co_return; 
}

