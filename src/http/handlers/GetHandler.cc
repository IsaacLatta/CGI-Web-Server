#include "MethodHandler.h"
#include "../Session.h"
#include "../../Streamer.h"
#include "../Exception.h"

asio::awaitable<void> GetHandler::handleScript() {
    bool first_read = true;
    auto chunk_callback =
        [this, first_read, response = txn_.GetResponse(), sock = txn_.GetSocket()](const char* buf, std::size_t len) mutable -> asio::awaitable<void> {
            if(!first_read) {
                co_return;
            }
            std::span<const char> buffer(buf, len);
            if((response.Status = http::extract_status_code(buffer)) == http::Code::Bad_Request || response.Status == http::Code::Not_A_Status) {
                throw http::Exception(http::Code::Bad_Gateway,
                std::format("Failed to parse response from script"));
            }

            std::string response_str = response.SetBody(std::string(http::extract_body(buffer)))
                    .AddHeaders(http::extract_headers(buffer))
                    .AddHeader("Connection", "close")
                    .Build();

            std::span<const char> header_buf(response_str.data(), response_str.length());
            http::WriteStatus result = co_await http::co_write_all(sock, header_buf);
            if(!http::is_success_code(result.status)) {
                throw http::Exception(result.status, std::move(result.message));
            }
            co_return;
    };

    const auto& request = txn_.GetRequest();
    std::string script = request.endpoint->getResource(request.method);
    std::string args(request.args);
    ScriptStreamer streamer(script, args, chunk_callback);
    co_await streamer.stream(txn_.GetSocket());
    txn_.addBytes(streamer.getBytesStreamed());
    co_return;
}

asio::awaitable<void> GetHandler::handleFile() {
    std::string file = txn_.GetRequest().route->resource;
    std::string content_type;
    if(http::determine_content_type(file, content_type) != http::Code::OK) {
        throw http::Exception(http::Code::Forbidden, std::format("Failed to extract content_type for endpoint={}, file={}",
            txn_.GetRequest().endpoint_url, file));
    }

    txn_.GetResponse()
        .SetStatus(http::Code::OK)
        .AddHeader("Connection", "close")
        .AddHeader("Content-Type", content_type);

    std::string response_header = txn_.GetResponse().Build();
    StringStreamer s_stream(&response_header);
    co_await s_stream.Stream(txn_.GetSocket());

    FileStreamer f_stream(file);
    txn_.GetResponse().AddHeader("Content-Type", std::to_string(f_stream.GetFileSize()));
    co_await f_stream.Stream(txn_.GetSocket());
    txn_.addBytes(f_stream.getBytesStreamed() + s_stream.getBytesStreamed());
    co_return;
}

asio::awaitable<void> GetHandler::Handle() {
    const auto& request = txn_.GetRequest();

    if(!request.endpoint || !request.route) {
        throw http::Exception(http::Code::Service_Unavailable, 
        std::format("No GET route found for endpoint={}", request.endpoint_url));
    }
    
    if(request.route->has_script) {
        co_await handleScript();
        co_return;
    } 
    
    co_await handleFile();
    co_return;    
}
