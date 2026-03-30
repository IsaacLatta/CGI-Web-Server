#include "MethodHandler.h"
#include "http/Session.h"
#include "http/Exception.h"
#include "Streamer.h"
#include "io/io.h"

asio::awaitable<void> PostHandler::Handle() {
    auto& request = txn_.GetRequest();

    if (!txn_.Endpoint || !txn_.Endpoint->HasScript) {
        throw http::Exception(http::Service_Unavailable,
            std::format("No POST route found for endpoint={}", request.GetPath());
    }

    bool first_read { true };
    auto chunk_callback =
        [first_read, response = txn_.GetResponse(), &sock = txn_.GetSocket()] (const char* buf, size_t len) mutable -> asio::awaitable<void> {
            if(!first_read) {
                co_return;
            }
            std::span<const char> buffer(buf, len);
            if((response.Status = http::extract_status_code(buffer)) == http::Bad_Request || response.Status == http::Not_A_Status) {
                throw http::Exception(http::Bad_Gateway,
                std::format("Failed to parse response from script"));
            }

            std::string response_str = response.SetBody(std::string(http::extract_body(buffer)))
                    .AddHeaders(http::extract_headers(buffer))
                    .AddHeader("Connection", "close")
                    .Build();

            std::span<const char> header_buf(response_str.data(), response_str.length());
            const auto result = co_await io::co_write_all(sock, header_buf);
            if (result.ec) {
                throw http::Exception(http::error_to_status(result.ec));
            }

            co_return;
        };

    ScriptStreamer streamer(txn_.Endpoint->ResourceName, std::string(request.GetQueryString()), chunk_callback);
    co_await streamer.stream(txn_.GetSocket());
    txn_.addBytes(streamer.getBytesStreamed());
    co_return; 
}

