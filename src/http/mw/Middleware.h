#pragma once

#include <functional>
#include <memory>
#include <utility>
#include <vector>

#include <asio.hpp>
#include <asio/awaitable.hpp>

#include "http/forward.h"

namespace mw {

template<typename Context>
class Middleware {
public:
    using NextCallback = std::function<asio::awaitable<void>(Context&)>;
    using FinishCallback = std::function<asio::awaitable<void>(Context&, http::Response, std::optional<http::Handler>)>;

public:
    virtual ~Middleware() = default;
    virtual asio::awaitable<void> Process(Context&, NextCallback, FinishCallback) = 0;
};

template<typename Context>
class Pipeline {
public:
    using MiddlewareType = Middleware<Context>;
    using FinishCallback = typename MiddlewareType::FinishCallback;
    using NextCallback = typename MiddlewareType::NextCallback;

public:
    asio::awaitable<void> Run(Context& ctx, const FinishCallback& on_finish) const {
        co_await RunOne(ctx, 0u, on_finish);
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
    asio::awaitable<void> RunOne(Context& context, size_t index, const FinishCallback& on_finish) const {
        if (index == components_.size()) {
            co_return;
        }

        NextCallback next = [this, index, on_finish](auto& ctx) mutable -> asio::awaitable<void> {
            co_await RunOne(ctx, index + 1, std::move(on_finish));
        };

        co_return co_await components_.at(index)->Process(context, next, on_finish);
    }

private:
    std::vector<std::unique_ptr<MiddlewareType>> components_;
};

template<typename Context>
using FinishCallback = typename Middleware<Context>::FinishCallback;

template<typename Context>
using NextCallback = typename Middleware<Context>::Next;

} // namespace mw