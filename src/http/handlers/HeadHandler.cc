#include "http/handlers/MethodHandler.h"
#include "http/http.h"
#include "http/Session.h"

#include "io/Streamer.h"

asio::awaitable<void> HeadHandler::Handle(http::PostRouteContext& ctx) {
    auto [ec, file_size] = io::get_file_size(ctx.GetEndpoint().ResourceName);
    if (ec) {
        ctx.GetLogEntry().ErrorCode = ec;
        ctx.SetResponse(http::Response { http::Code::Internal_Server_Error });
        co_return co_await http::send_response(ctx.GetResponse(), ctx.GetSocket());
    }

    std::string content_type;
    const auto status = http::determine_content_type(ctx.GetEndpoint().ResourceName, content_type);
    if(status != http::OK) {
        ctx.SetResponse(http::Response { status });
        co_return co_await http::send_response(ctx.GetResponse(), ctx.GetSocket());
    }

    ctx.GetResponse()
        .SetStatus(http::OK)
        .AddHeader("Connection", "close")
        .AddHeader("Content-Type", content_type)
        .AddHeader("Content-Length", std::to_string(file_size));

    auto [err, bytes] = co_await http::send_response_to(ctx.GetResponse(), ctx.GetSocket());
    if (err) {
        ctx.GetLogEntry().ErrorCode = err;
        ctx.GetLogEntry().BytesServed += bytes;
    }
    co_return;
}
