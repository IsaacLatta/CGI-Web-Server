#include "MethodHandler.h"
#include "io/Streamer.h"
#include "http/http.h"
#include "http/mw/Context.h"

asio::awaitable<void> GetScriptHandler::Handle(http::PostRouteContext& ctx) {
    io::ScriptStreamer streamer(ctx.GetEndpoint(). ResourceName, std::string(ctx.GetRequest().GetQueryString()));

    auto [ec, _] = streamer.OpenStream();
    if (ec) {
        ctx.SetResponse(http::Response { http::Code::Bad_Gateway });
        io::StringStreamer str_streamer(ctx.GetResponse().Build());
        co_await str_streamer.Stream(ctx.GetSocket());
        co_return;
    }

    co_await streamer.Stream(ctx.GetSocket());
    ctx.GetLogEntry().BytesServed += streamer.GetBytesStreamed();
    co_return;
}

asio::awaitable<void> GetFileHandler::Handle(http::PostRouteContext& ctx) {
    std::string content_type;
    const auto status = http::determine_content_type(ctx.GetEndpoint().ResourceName, content_type);
    if(status != http::Code::OK) {
        ctx.SetResponse(http::Response { status });
        co_return co_await http::send_response(ctx.GetResponse(), ctx.GetSocket());
    }

    auto result = io::get_file_size(ctx.GetEndpoint().ResourceName);
    if (result.ec) {
        ctx.GetLogEntry().ErrorCode = result.ec;
        ctx.GetLogEntry().BytesServed += result.bytes;
        co_return;
    }

    ctx.GetResponse()
        .SetStatus(http::Code::OK)
        .AddHeader("Connection", "close")
        .AddHeader("Content-Length", std::to_string(result.bytes))
        .AddHeader("Content-Type", content_type).Build();

    result = co_await http::send_response_to(ctx.GetResponse(), ctx.GetSocket());
    if (result.ec) {
        ctx.GetLogEntry().ErrorCode = result.ec;
        ctx.GetLogEntry().BytesServed += result.bytes;
        co_return;
    }

    io::FileStreamer f_stream(ctx.GetEndpoint().ResourceName);
    result = f_stream.OpenStream();
    if (result.ec) {
        ctx.GetLogEntry().ErrorCode = result.ec;
        ctx.GetLogEntry().BytesServed += result.bytes;
        co_return;
    }

    result = co_await f_stream.Stream(ctx.GetSocket());
    if (result.ec) {
        ctx.GetLogEntry().ErrorCode = result.ec;
        ctx.GetLogEntry().BytesServed += result.bytes;
        co_return;
    }
    ctx.GetLogEntry().BytesServed += result.bytes;
    co_return;
}

