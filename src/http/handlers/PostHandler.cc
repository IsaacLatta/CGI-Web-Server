#include "MethodHandler.h"
#include "http/Session.h"
#include "http/Exception.h"
#include "io/Streamer.h"
#include "io/io.h"

asio::awaitable<void> PostHandler::Handle(http::PostRouteContext& ctx) {
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

