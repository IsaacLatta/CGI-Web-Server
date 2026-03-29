#pragma once

#include <string>

#include <asio.hpp>

#include "io/Socket.h"
#include "Transaction.h"

#include "http/forward.h"

class MethodHandler {
public:
    virtual ~MethodHandler() = default;
    virtual asio::awaitable<void> Handle() = 0;
};

class GetHandler: public MethodHandler {
public:
    GetHandler(Transaction& txn) : txn_(txn) {}
    
    asio::awaitable<void> Handle() override;

private:
    asio::awaitable<void> handleScript();
    asio::awaitable<void> handleFile();

private:
    Transaction& txn_;
};

class HeadHandler: public MethodHandler {
public:
    HeadHandler(Transaction& txn) : txn_(txn) {}
    
    asio::awaitable<void> Handle() override;

private:
    void buildResponse();

private:
    Transaction& txn_;
};

class PostHandler: public MethodHandler  {
public:
    PostHandler(Transaction& txn) : txn_(txn) {}
    
    asio::awaitable<void> Handle() override;

private:
    Transaction& txn_;
    long total_bytes { 0 };
    std::string response_header;
};

class OptionsHandler: public MethodHandler {
public:
    OptionsHandler(Transaction& txn): txn_(txn) {};

    asio::awaitable<void> Handle() override;

private:
    Transaction& txn_;
};
