#pragma once

#include <string>

#include <asio.hpp>

#include "io/Socket.h"

#include "http/forward.h"

class MethodHandler {
public:
    virtual ~MethodHandler() = default;
    virtual asio::awaitable<void> Handle(http::PostRouteContext&) = 0;
};

class GetFileHandler: public MethodHandler {
public:
    asio::awaitable<void> Handle(http::PostRouteContext&) override;
};

class GetScriptHandler: public MethodHandler {
public:
    asio::awaitable<void> Handle(http::PostRouteContext&) override;
};

class HeadHandler: public MethodHandler {
public:
    asio::awaitable<void> Handle(http::PostRouteContext&) override;
};

class PostHandler: public MethodHandler  {
public:
    asio::awaitable<void> Handle(http::PostRouteContext&) override;
};

class OptionsHandler: public MethodHandler {
public:
    asio::awaitable<void> Handle(http::PostRouteContext&) override;
};
