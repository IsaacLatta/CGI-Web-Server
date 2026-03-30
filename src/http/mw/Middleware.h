#pragma once

#include <asio.hpp>
#include <asio/awaitable.hpp>
#include "http/Transaction.h"

#include "http/forward.h"

namespace mw {

class Middleware {
public:
    virtual ~Middleware() = default;
    virtual asio::awaitable<void> Process(http::Transaction& txn, Next next) = 0;
};

class NoOpMiddleware : public Middleware {
public:
    NoOpMiddleware() = default;

    asio::awaitable<void> Process(http::Transaction& txn, Next next) override {
        if (next) {
            co_await next();
        }
    }
};

class Pipeline {
public:
    asio::awaitable<void> Run(http::Transaction&) const;

    template<typename Component, typename... Args>
    Pipeline& AddComponent(Args&&... args) {
        components_.emplace_back(std::make_unique<Component>(std::forward<Args>(args)...));
        return *this;
    }

    Pipeline& AddComponent(std::unique_ptr<Middleware> middleware) {
        components_.emplace_back(std::move(middleware));
        return *this;
    }

private:
    asio::awaitable<void> RunOne(http::Transaction&, size_t index) const;

private:
    std::vector<std::unique_ptr<Middleware>> components_;
};

} // namespace mw