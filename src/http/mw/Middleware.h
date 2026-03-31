#pragma once

#include <asio.hpp>
#include <asio/awaitable.hpp>

#include "http/forward.h"

namespace mw {

template<typename Context>
class Middleware {
public:
    using NextCallback = std::function<asio::awaitable<void>(Context&)>;
    using FinishCallback = std::function<asio::awaitable<void>(Context&, http::Response)>;

public:
    virtual ~Middleware() = default;
    virtual asio::awaitable<void> Process(Context&, NextCallback, FinishCallback) = 0;
};

template<typename Context>
class Pipeline {
public:
    using MiddlewareType = Middleware<Context>;
    using FinishCallback = MiddlewareType::FinishCallback;
    using CompletionCallback = MiddlewareType::NextCallback;
    using NextCallback = MiddlewareType::NextCallback;

public:
    asio::awaitable<void> Run(Context& ctx, FinishCallback on_finish, CompletionCallback on_complete) const {
        co_await RunOne(ctx, 0u, std::move(on_finish), std::move(on_complete));
    }

    template<typename Component, typename... Args>
    Pipeline& AddComponent(Args&&... args) {
        components_.emplace_back(std::make_unique<Component>(std::forward<Args>(args)...));
        return *this;
    }

    Pipeline& AddComponent(std::unique_ptr<MiddlewareType> middleware) {
        components_.emplace_back(std::move(middleware));
        return *this;
    }

private:
    asio::awaitable<void> RunOne(Context& context, size_t index, FinishCallback on_finish, CompletionCallback on_complete) const {
        if (index == components_.size()) {
            co_return co_await on_complete(context);
        }

        NextCallback next = [this, index, complete_cb = std::move(on_complete), on_finish](auto& ctx) mutable -> asio::awaitable<void> {
            co_await RunOne(ctx, index + 1, on_finish, std::move(complete_cb));
        };

        co_await components_.at(index)->Process(context, next, on_finish);
    }

private:
    std::vector<std::unique_ptr<MiddlewareType>> components_;
};

} // namespace mw