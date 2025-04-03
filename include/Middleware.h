#ifndef MIDDLEWARE_H
#define MIDDLEWARE_H

#include <asio.hpp>
#include <asio/awaitable.hpp>
#include <exception>
#include "Transaction.h"
#include "MethodHandler.h"
#include "config.h"
#include "logger_macros.h"
#include "http.h"

class Session;

namespace mw {

using Next = std::function<asio::awaitable<void>()>;

class Middleware
{
    public:
    Middleware(): config(cfg::Config::getInstance()) {}
    virtual asio::awaitable<void> process(Transaction* txn, Next next) = 0;
    virtual ~Middleware() = default;
    protected:
    const cfg::Config* config;
};

class ErrorHandler: public Middleware 
{
    public:
    asio::awaitable<void> process(Transaction* txn, Next next) override;  
};

class RequestHandler: public Middleware
{
    public:
    asio::awaitable<void> process(Transaction* txn, Next next) override;
    private:
    std::shared_ptr<MethodHandler> createMethodHandler(Transaction* txn);
};

class Logger: public Middleware
{
    public:
    asio::awaitable<void> process(Transaction* txn, Next next) override;
    private:
    bool forward{true};
};

class Parser: public Middleware
{
    public:
    asio::awaitable<void> process(Transaction* txn, Next next) override;
    private:
};

class Authenticator: public Middleware
{
    public:
    asio::awaitable<void> process(Transaction* txn, Next next) override;
    private:
    void validate(Transaction* txn, const http::Endpoint* endpoint);
};

};
#endif