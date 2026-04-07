#include "http/Session.h"
#include "http/Exception.h"
#include "http/mw/Middleware.h"
#include "http/mw/Context.h"
#include "http/routing/Router.h"
#include "http/routing/Route.h"
#include "logger/macros.h"

namespace http {

asio::awaitable<void> DefaultSession::Start() {
    Response response;
    try {
        co_return co_await DoPeRoute();
    } catch (const Exception& e) {
        response = e.GetResponse();
    } catch (const std::exception& e) {
        response = Response(Internal_Server_Error);
    } catch (...) {
        response = Response(Internal_Server_Error);
    }

    co_return co_await OnFinish(response, {});
}

asio::awaitable<void> DefaultSession::DoPeRoute() {
    PreRouteContext context(*socket_, buffer_);
    co_await pipeline_.Run(context,
        [this](PreRouteContext&, Response response) -> asio::awaitable<void> {
            co_return co_await OnFinish(std::move(response));
        });

    if (done_) {
        co_return;
    }

    co_return co_await DoPostRoute(context);
}

asio::awaitable<void> DefaultSession::DoPostRoute(const PreRouteContext& context) {
    if (!context.MatchedRoute || !context.MatchedEndpoint || !context.ParsedRequest) {
        co_return co_await OnFinish(Response(Internal_Server_Error), {});
    }

    auto request = std::move(*context.ParsedRequest);
    PostRouteContext post_route_context(
        *context.MatchedRoute,
        *context.MatchedEndpoint,
        *socket_,
        request,
        buffer_
    );

    const auto& route_pipeline = post_route_context.MatchedRoute.Pipeline();
    co_await route_pipeline.Run(post_route_context,
        [this](auto&, Response response, auto handler) -> asio::awaitable<void> {
            co_return co_await OnFinish(std::move(response), std::move(handler));
        });

    if (done_) {
        co_return;
    }

    co_await post_route_context.MatchedEndpoint.Pipeline.Run(
        post_route_context,
        [this](auto&, Response response, auto handler) -> asio::awaitable<void> {
            co_return co_await OnFinish(std::move(response), std::move(handler));
        });

    if (done_) {
        co_return;
    }

    if (!post_route_context.FinalHandler) {
        co_return co_await OnFinish(Response(Internal_Server_Error), {});
    }

    co_await post_route_context.FinalHandler(post_route_context.WorkingResponse);
    done_ = true;
}

asio::awaitable<void> DefaultSession::OnFinish(Response response, std::optional<Handler> handler) {
    try {
        if (done_) {
            co_return;
        }
        done_ = true;

        if (handler) {
            co_return co_await handler.value()(response);
        }

        co_return co_await router_.GetErrorPage(response.Status).Finisher(response);
    } catch (const Exception& e) {
        ERROR("Server", "exception thrown in finisher for status=%d: %s", static_cast<int>(response.Status), e.what());
    } catch (const std::exception& e) {
        ERROR("Server", "exception thrown in finisher for status=%d: %s", static_cast<int>(response.Status), e.what());
    } catch (...) {
        ERROR("Server", "unknown exception thrown in finisher for status=%d", static_cast<int>(response.Status));
    }

    co_return;
}


}
