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

class GetHandler: public MethodHandler {
public:
    asio::awaitable<void> Handle(http::PostRouteContext&) override;

private:
    asio::awaitable<void> handleScript();
    asio::awaitable<void> handleFile();
};

class HeadHandler: public MethodHandler {
public:
    asio::awaitable<void> Handle(http::PostRouteContext&) override;

private:
    void buildResponse();
};

class PostHandler: public MethodHandler  {
public:
    asio::awaitable<void> Handle(http::PostRouteContext&) override;

private:
    size_t total_bytes { 0u };
    std::string response_header;
};

class OptionsHandler: public MethodHandler {
public:
    asio::awaitable<void> Handle(http::PostRouteContext&) override;
};
