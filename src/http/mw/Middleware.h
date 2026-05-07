#pragma once

#include <functional>
#include <memory>
#include <optional>
#include <utility>
#include <vector>
#include <cassert>

#include <asio.hpp>
#include <asio/awaitable.hpp>

#include "core/dbg.h"
#include "http/forward.h"
#include "http/Response.h"

namespace mw {

enum class PipelineOutcome {
    Finished,
    Continued,
}; using enum PipelineOutcome;

template<typename Context>
using NextCallback = std::function<asio::awaitable<void>(Context&)>;

template<typename Context>
using FinishCallback = std::function<asio::awaitable<void>(Context&, http::Response, std::optional<http::ResponseHandler>)>;

template<typename Context>
class Middleware {
public:
    using Next = NextCallback<Context>;
    using Finish = FinishCallback<Context>;

public:
    virtual ~Middleware() = default;

    virtual asio::awaitable<void> Process(Context&, Next, Finish) = 0;
};

template<typename Context>
class Pipeline {
public:
    using MiddlewareType = Middleware<Context>;
    using MiddlewarePtr = std::shared_ptr<MiddlewareType>;
    using FinishCallback = mw::FinishCallback<Context>;
    using NextCallback = mw::NextCallback<Context>;

public:
    asio::awaitable<PipelineOutcome> Run(Context& ctx, const FinishCallback& on_finish) const {
        bool finished { false };

        FinishCallback wrapped_finish =
            [&finished, on_finish](Context& ctx, http::Response response, std::optional<http::ResponseHandler> handler) -> asio::awaitable<void> {
                finished = true;
                co_return co_await on_finish(ctx, std::move(response), std::move(handler));
            };

        co_await RunOne(ctx, 0u, wrapped_finish, finished);
        co_return finished ? Finished : Continued;
    }

    template<typename Component, typename... Args>
    Pipeline& AddComponent(Args&&... args) {
        components_.emplace_back(std::make_shared<Component>(std::forward<Args>(args)...));
        return *this;
    }

    Pipeline& AddComponent(MiddlewarePtr middleware) {
        components_.emplace_back(std::move(middleware));
        return *this;
    }

private:
    asio::awaitable<void> RunOne(Context& context, size_t index, const FinishCallback& on_finish, bool& finished) const {
        if (finished || index == components_.size()) {
            co_return;
        }

        NextCallback next = [this, index, on_finish, &finished](Context& ctx) -> asio::awaitable<void> {
            DBG_ASSERT(!finished, "next called after finish callback");

            if (finished) {
                co_return;
            }

            co_return co_await RunOne(ctx, index + 1, on_finish, finished);
        };

        co_return co_await components_.at(index)->Process(context, std::move(next), on_finish);
    }

private:
    std::vector<MiddlewarePtr> components_;
};

} // namespace mw