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

struct IpInfo {
    std::atomic<std::uint64_t> window_and_count; 
}; 

class IPRateLimiter: public Middleware
{
    public:
    IPRateLimiter(cfg::RateSetting setting): setting(setting) {clients.reserve(2048);} // avoid reshashing, could possibly add expired client removal
    IPRateLimiter() {clients.reserve(2048);} // avoid reshashing, could possibly add expired client removal
    asio::awaitable<void> process(Transaction* txn, Next next) override;
    private:
    std::unordered_map<std::string, std::unique_ptr<IpInfo>> clients;
    cfg::RateSetting setting;
    std::mutex clients_mutex;

    private:
    IpInfo* findClient(const std::string& ip);
};

};
#endif