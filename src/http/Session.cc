#include "http/Session.h"
#include "http/Response.h"
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
    PreRouteContext ctx(state_);

    const auto outcome = co_await preroute_pipeline_.Run(
        ctx,
        [this](auto&, Response response, std::optional<ResponseHandler> handler) -> asio::awaitable<void> {
            co_return co_await OnFinish(std::move(response), std::move(handler));
        }
    );

    if (outcome == mw::Finished) {
        co_return;
    }

    if (!ctx.IsRouted()) {
        co_return co_await OnFinish(Response(Internal_Server_Error));
    }

    co_return co_await DoPostRoute();
}

asio::awaitable<void> DefaultSession::DoPostRoute() {
    PostRouteContext ctx(state_);

    const auto route_outcome = co_await ctx.GetRoute().Pipeline().Run(
        ctx,
        [this](auto&, Response response, std::optional<ResponseHandler> handler) -> asio::awaitable<void> {
            co_return co_await OnFinish(std::move(response), std::move(handler));
        }
    );

    if (route_outcome == mw::Finished) {
        co_return;
    }

    const auto endpoint_outcome = co_await ctx.GetEndpoint().Pipeline.Run(
        ctx,
        [this](auto&, Response response, std::optional<ResponseHandler> handler) -> asio::awaitable<void> {
            co_return co_await OnFinish(std::move(response), std::move(handler));
        }
    );

    if (endpoint_outcome == mw::Finished) {
        co_return;
    }

    co_await ctx.GetEndpoint().Handler(ctx);
    co_return;
}

asio::awaitable<void> DefaultSession::OnFinish(Response response, std::optional<ResponseHandler> handler) {
    try {
        if (handler) {
            co_await (*handler)(response);
        } else {
            if (response.IsSuccess()) {
                response = Response { Internal_Server_Error };
            }

            co_await router_.GetErrorPage(response.Status).Handler(response);
        }

        FinalContext context(state_, response);

        auto do_nothing = [](auto&, Response, std::optional<ResponseHandler>) -> asio::awaitable<void> { co_return; };
        co_await final_pipeline_.Run(context, do_nothing);
        co_return;
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
