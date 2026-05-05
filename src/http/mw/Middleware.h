#pragma once

#include <functional>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include <asio.hpp>
#include <asio/awaitable.hpp>

#include "http/forward.h"

namespace mw {

enum class PipelineOutcome {
    Finished,
    Continued,
}; using enum PipelineOutcome;

template<typename Context>
using NextCallback = std::function<asio::awaitable<void>(Context&)>;

template<typename Context>
using FinishCallback = std::function<
    asio::awaitable<void>(
        Context&,
        http::Response,
        std::optional<http::Handler>
    )
>;

template<typename Context>
class Middleware {
public:
    using Next = NextCallback<Context>;
    using Finish = FinishCallback<Context>;

public:
    virtual ~Middleware() = default;

    virtual asio::awaitable<void> Process(
        Context&,
        Next,
        Finish
    ) = 0;
};

template<typename Context>
class Pipeline {
public:
    using MiddlewareType = Middleware<Context>;
    using FinishCallback = mw::FinishCallback<Context>;
    using NextCallback = mw::NextCallback<Context>;

public:
    asio::awaitable<PipelineOutcome> Run(
        Context& ctx,
        const FinishCallback& on_finish
    ) const {
        bool finished = false;

        FinishCallback wrapped_finish =
            [&finished, on_finish](
                Context& ctx,
                http::Response response,
                std::optional<http::Handler> handler
            ) -> asio::awaitable<void> {
                finished = true;

                co_return co_await on_finish(
                    ctx,
                    std::move(response),
                    std::move(handler)
                );
            };

        co_await RunOne(ctx, 0u, wrapped_finish);

        co_return finished
            ? PipelineOutcome::Finished
            : PipelineOutcome::Continued;
    }

    template<typename Component, typename... Args>
    Pipeline& AddComponent(Args&&... args) {
        components_.emplace_back(
            std::make_unique<Component>(std::forward<Args>(args)...)
        );

        return *this;
    }

    Pipeline& AddComponent(std::unique_ptr<MiddlewareType> middleware) {
        components_.emplace_back(std::move(middleware));
        return *this;
    }

private:
    asio::awaitable<void> RunOne(
        Context& context,
        size_t index,
        const FinishCallback& on_finish
    ) const {
        if (index == components_.size()) {
            co_return;
        }

        NextCallback next =
            [this, index, on_finish](Context& ctx) -> asio::awaitable<void> {
                co_return co_await RunOne(ctx, index + 1, on_finish);
            };

        co_return co_await components_.at(index)->Process(
            context,
            std::move(next),
            on_finish
        );
    }

private:
    std::vector<std::unique_ptr<MiddlewareType>> components_;
};

} // namespace mw