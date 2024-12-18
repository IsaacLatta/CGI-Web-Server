#ifndef MIDDLEWARE_H
#define MIDDLEWARE_H

#include <asio.hpp>
#include <asio/awaitable.hpp>


class Session;

using Next = std::function<asio::awaitable<void>()>;

class Middleware
{
    public:
    virtual asio::awaitable<void> process(std::shared_ptr<Session> session, Next next) = 0;
    virtual ~Middleware() = default;
};

class ErrorHandlerMiddleware: public Middleware 
{
    public:
    asio::awaitable<void> process(std::shared_ptr<Session> session, Next next) override;
    
};

class RequestHandler: public Middleware
{
    public:
    asio::awaitable<void> process(std::shared_ptr<Session> session, Next next) override;
};

#endif