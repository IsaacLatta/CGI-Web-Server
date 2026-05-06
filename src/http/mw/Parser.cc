#include "http/mw/Parser.h"
#include "http/Exception.h"
#include "http/parsing/parse.h"
#include "http/routing/Router.h"
#include "logger/macros.h"

namespace mw {

asio::awaitable<void> Parser::Process(http::PreRouteContext& context, Next next, Finish finish) {
    auto [ec, bytes] = co_await context.GetSocket().Read(context.GetBuffer());
    if(ec) {
        co_return co_await finish(context, http::Response(http::Client_Closed_Request), std::nullopt);
    }
    context.GetBuffer().resize(bytes);

    http::Request request;
    request.SetPath(http::extract_endpoint(context.GetBuffer()))
        .SetMethod(http::extract_method(context.GetBuffer()))
        .SetHeaders(http::extract_headers(context.GetBuffer()))
        .SetQueryParams(http::extract_query_params(context.GetBuffer()))
        .SetBody(http::extract_body(context.GetBuffer()));

    TRACE("MW Parser", "Hit for endpoint: %s", request.GetPath().c_str());

    auto& route = router_.GetRoute(request.GetPath());
    context.SetRoute(route);
    context.SetEndpoint(route.GetEndpoint(request.GetMethod()));
    context.SetRequest(std::move(request));

    co_return co_await next(context);
}

}