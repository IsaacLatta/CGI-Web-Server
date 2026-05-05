#include "MethodHandler.h"
#include "logger/macros.h"

#include "http/mw/Context.h"

static std::string get_methods_str(const std::vector<http::Method>& methods) {
    std::string allow_header;
    for(const auto method : methods) {
        allow_header += std::string(http::method_enum_to_str(method)) + ", ";
    }
    if(!allow_header.empty()) {
        allow_header.erase(allow_header.size() - 2, 2);
    }
    return allow_header;
}

asio::awaitable<void> OptionsHandler::Handle(http::PostRouteContext& ctx) {
    std::string response_str = ctx.GetResponse()
        .AddHeader("Allow", get_methods_str(ctx.GetRoute().GetAvailableMethods()))
        .AddHeader("Content-Length", "0")
        .AddHeader("Connection", "close")
        .Build();

    auto [ec, bytes] = co_await ctx.GetState().Socket->Write(response_str);
    ctx.GetLogEntry().error_code = ec;
    ctx.GetLogEntry().BytesServed += bytes;
    co_return;
}
