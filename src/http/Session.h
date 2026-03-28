#pragma once

#include <asio.hpp>
#include <asio/awaitable.hpp>

#include "io/Socket.h"

#include "http/forward.h"

namespace http {

class Session {
public:
    virtual ~Session() = default;

    virtual asio::awaitable<void> Start() = 0;
};

class DefaultSession : public Session {
public:
    explicit DefaultSession(io::SocketPtr&& sock) : sock_(std::move(sock)){};

    asio::awaitable<void> Start() override;

private:
    io::SocketPtr sock_;
};

} // namespace http

