#include "http/mw/Parser.h"
#include "http/Exception.h"
#include "http/parsing/parse.h"
#include "http/routing/Router.h"
#include "logger/macros.h"

namespace mw {

asio::awaitable<void> Parser::Process(http::PreRouteContext& context, NextCallback next, FinishCallback finish) {
    auto [ec, bytes] = co_await context.Socket.Read(context.Buffer);
    if(ec) {
        co_return co_await finish(context, http::Response(http::Client_Closed_Request));
    }
    context.Buffer.resize(bytes);

    http::Request request;
    request.SetPath(http::extract_endpoint(context.Buffer))
        .SetMethod(http::extract_method(context.Buffer))
        .SetHeaders(http::extract_headers(context.Buffer))
        .SetQueryParams(http::extract_query_params(context.Buffer))
        .SetBody(http::extract_body(context.Buffer));

    TRACE("MW Parser", "Hit for endpoint: %s", request.GetPath().c_str());

    context.MatchedRoute = &router_.GetRoute(request.GetPath());
    context.MatchedEndpoint = &context.MatchedRoute->GetEndpoint(request.GetMethod());
    context.ParsedRequest = std::move(request);

    co_return co_await next(context);
}

}