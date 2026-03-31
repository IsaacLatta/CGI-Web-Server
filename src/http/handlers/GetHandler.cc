#include "MethodHandler.h"
#include "../Session.h"
#include "../../Streamer.h"
#include "../Exception.h"

asio::awaitable<void> GetHandler::handleScript() {
    co_return;
    // bool first_read { true };
    // auto chunk_callback =
    //     [&first_read, response = txn_.GetResponse(), &sock = txn_.GetSocket()](const char* buf, std::size_t len) mutable -> asio::awaitable<void> {
    //         if(!first_read) {
    //             co_return;
    //         }
    //         first_read = false;
    //
    //         std::span<const char> buffer(buf, len);
    //         if((response.Status = http::extract_status_code(buffer)) == http::Code::Bad_Request || response.Status == http::Code::Not_A_Status) {
    //             throw http::Exception(http::Code::Bad_Gateway,
    //             std::format("Failed to parse response from script"));
    //         }
    //
    //         std::string response_str = response.SetBody(std::string(http::extract_body(buffer)))
    //                 .AddHeaders(http::extract_headers(buffer))
    //                 .AddHeader("Connection", "close")
    //                 .Build();
    //
    //         std::span<const char> header_buf(response_str.data(), response_str.length());
    //         const auto result = co_await io::co_write_all(sock, header_buf);
    //         if (result.ec) {
    //             throw http::Exception(http::error_to_status(result.ec));
    //         }
    //         co_return;
    // };
    //
    // // TODO: Need to fix arg parsing up stream, before this is implemented
    //
    // const auto& request = txn_.GetRequest();
    // std::string args = txn_.ResolvedEndpoint->ArgType ==
    // ScriptStreamer streamer(txn_.ResolvedEndpoint->ResourceName, args, chunk_callback);
    // co_await streamer.stream(txn_.GetSocket());
    // txn_.addBytes(streamer.getBytesStreamed());
    // co_return;
}

asio::awaitable<void> GetHandler::handleFile() {
    const std::string& file = txn_.ResolvedEndpoint->ResourceName;
    std::string content_type;
    if(http::determine_content_type(file, content_type) != http::Code::OK) {
        throw http::Exception(http::Code::Forbidden, std::format("Failed to extract content_type for endpoint={}, file={}",
            txn_.GetRequest().GetPath(), file));
    }

    txn_.GetResponse()
        .SetStatus(http::Code::OK)
        .AddHeader("Connection", "close")
        .AddHeader("Content-Type", content_type);

    const std::string response_header = txn_.GetResponse().Build();
    StringStreamer s_stream(response_header);
    co_await s_stream.Stream(txn_.GetSocket());

    FileStreamer f_stream(file);
    txn_.GetResponse().AddHeader("Content-Type", std::to_string(f_stream.GetFileSize()));
    co_await f_stream.Stream(txn_.GetSocket());
    txn_.addBytes(f_stream.BytesStreamed() + s_stream.BytesStreamed());
    co_return;
}

asio::awaitable<void> GetHandler::Handle() {
    if (!txn_.ResolvedEndpoint || !txn_.ResolvedRoute) {
        throw http::Exception(http::Code::Internal_Server_Error, "routing failure in get handler");
    }

    if(txn_.ResolvedEndpoint->HasScript) {
        co_await handleScript();
        co_return;
    } 
    
    co_await handleFile();
    co_return;    
}
