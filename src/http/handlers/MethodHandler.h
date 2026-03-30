#pragma once

#include <string>

#include <asio.hpp>

#include "io/Socket.h"

#include "http/forward.h"

class MethodHandler {
public:
    virtual ~MethodHandler() = default;
    virtual asio::awaitable<void> Handle() = 0;
};

class GetHandler: public MethodHandler {
public:
    GetHandler(http::Transaction& txn) : txn_(txn) {}
    
    asio::awaitable<void> Handle() override;

private:
    asio::awaitable<void> handleScript();
    asio::awaitable<void> handleFile();

private:
    http::Transaction& txn_;
};

class HeadHandler: public MethodHandler {
public:
    HeadHandler(http::Transaction& txn) : txn_(txn) {}
    
    asio::awaitable<void> Handle() override;

private:
    void buildResponse();

private:
    http::Transaction& txn_;
};

class PostHandler: public MethodHandler  {
public:
    PostHandler(http::Transaction& txn) : txn_(txn) {}
    
    asio::awaitable<void> Handle() override;

private:
    http::Transaction& txn_;
    size_t total_bytes { 0u };
    std::string response_header;
};

class OptionsHandler: public MethodHandler {
public:
    OptionsHandler(http::Transaction& txn): txn_(txn) {};

    asio::awaitable<void> Handle() override;

private:
    http::Transaction& txn_;
};
