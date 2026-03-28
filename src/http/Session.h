
#ifndef SESSION_H
#define SESSION_H

#include <asio.hpp>
#include <asio/ssl.hpp>
#include <vector>
#include <memory>
#include <iostream>
#include <chrono>

#include "MethodHandler.h"
#include "../logger/logger.h"
#include "http.h"
#include "config.h"
#include "Middleware.h"

#include "io/Socket.h"

#include "http/forward.h"

namespace http {

class Session {
public:
    virtual ~Session() = default;
    virtual ::io::Socket& GetSocket() = 0;
    virtual asio::awaitable<void> Start();
};

class DefaultSession : public Session {
public:
    explicit DefaultSession(::io::SocketPtr&& sock) : sock_(std::move(sock)){};

    asio::awaitable<void> Start() override;

    ::io::Socket& GetSocket() override {
        return *sock_;
    }

private:
    ::io::SocketPtr sock_;
};

} // namespace http

#endif
