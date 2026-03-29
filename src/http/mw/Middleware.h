#pragma once

#include <asio.hpp>
#include <asio/awaitable.hpp>
#include "Transaction.h"
#include "config.h"

#include "http/forward.h"

class DefaultSession;

namespace mw {

class Middleware {
public:
    virtual ~Middleware() = default;
    virtual asio::awaitable<void> Process(Transaction& txn, Next next) = 0;

    // TODO: get rid of these
    Middleware(): config(cfg::Config::getInstance()) {}
protected:
    const cfg::Config* config;
};

class Pipeline {
public:
    asio::awaitable<void> Run(Transaction&) const;

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
    asio::awaitable<void> RunOne(Transaction&, size_t index) const;

private:
    std::vector<std::unique_ptr<Middleware>> components_;
};

} // namespace mw