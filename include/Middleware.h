#ifndef MIDDLEWARE_H
#define MIDDLEWARE_H

#include <asio.hpp>
#include <asio/awaitable.hpp>
#include <exception>
#include "Transaction.h"
#include "MethodHandler.h"

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
    private:
    std::unique_ptr<MethodHandler> createMethodHandler(Transaction* txn);
};

class LoggingMiddleware: public Middleware
{
    public:
    asio::awaitable<void> process(Transaction* txn, Next next) override;
    private:
    bool forward{true};
};

class ParserMiddleware: public Middleware
{
    public:
    asio::awaitable<void> process(Transaction* txn, Next next) override;
    private:
};

class AuthenticatorMiddleware: public Middleware
{
    public:
    asio::awaitable<void> process(Transaction* txn, Next next) override;
    private:
};


#endif