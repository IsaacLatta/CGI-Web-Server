#pragma once

#include <asio.hpp>
#include <asio/awaitable.hpp>

#include "io/Socket.h"

#include "http/forward.h"
#include "Transaction.h"

namespace http {

class Session {
public:
    virtual ~Session() = default;

    virtual asio::awaitable<void> Start() = 0;
};

class DefaultSession : public Session {
public:
    explicit DefaultSession(
        const Router& router,
        const mw::Pipeline<PreRouteContext>& pipeline,
        io::SocketPtr&& sock)
        : router_(router), pipeline_(pipeline), socket_(std::move(sock)) {
        buffer_.reserve(io::BUFFER_SIZE);
        buffer_.resize(io::BUFFER_SIZE);
    }

    asio::awaitable<void> Start() override;

private:
    asio::awaitable<void> DoPeRoute();
    asio::awaitable<void> DoPostRoute(const PreRouteContext&);
    asio::awaitable<void> OnFinish(Response, std::optional<Handler>);

private:
    const Router& router_;
    const mw::Pipeline<PreRouteContext>& pipeline_;
    io::SocketPtr socket_;
    std::vector<char> buffer_;
    bool done_ { false };
};

} // namespace http

