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
    explicit DefaultSession(const mw::Pipeline& pipeline, io::SocketPtr&& sock)
        : pipeline_(pipeline), transaction_(std::move(sock)){};

    asio::awaitable<void> Start() override;

private:
    const mw::Pipeline& pipeline_;
    Transaction transaction_;
};

} // namespace http

