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

struct Pipeline {
    std::vector<std::unique_ptr<Middleware>> components;
    asio::awaitable<void> run(Transaction* txn);
};

class ErrorHandler: public Middleware 
{
    public:
    asio::awaitable<void> process(Transaction* txn, Next next) override;  
};

class Logger: public Middleware
{
    public:
    asio::awaitable<void> process(Transaction* txn, Next next) override;
    private:
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
    void validate(Transaction* txn, const http::EndpointMethod* route);
};

// struct IpInfo {
//     std::string ip;
//     cfg::RateSetting setting;
//     std::size_t counter{0};
// };

// class IPRateLimiter: public Middleware
// {
//     public:
//     asio::awaitable<void> process(Transaction* txn, Next next) override;
//     private:
//     static std::unordered_map<std::string, IpInfo> clients;
// };

};
#endif