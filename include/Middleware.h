#ifndef MIDDLEWARE_H
#define MIDDLEWARE_H

#include <asio.hpp>
#include <asio/awaitable.hpp>
#include <exception>
#include "Transaction.h"

class Session;

using Next = std::function<asio::awaitable<void>()>;

class Middleware
{
    public:
    virtual asio::awaitable<void> process(Transaction* txn, Next next) = 0;
    virtual ~Middleware() = default;
};

class ErrorHandlerMiddleware: public Middleware 
{
    public:
    asio::awaitable<void> process(Transaction* txn, Next next) override;  
};

class RequestHandlerMiddleware: public Middleware
{
    public:
    asio::awaitable<void> process(Transaction* txn, Next next) override;
};

class LoggingMiddleware: public Middleware
{
    public:
    asio::awaitable<void> process(Transaction* txn, Next next) override;
    private:
    bool forward{false};
};

#endif