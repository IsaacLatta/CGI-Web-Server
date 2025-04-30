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

class RateLimiter: public Middleware
{
    asio::awaitable<void> process(Transaction* txn, Next next) override;
};

struct IpInfo {
    /* upper 32 bits window, lower 32 bits counter */
    std::atomic<std::uint64_t> window_and_count; 
}; 

class FixedWindowLimiter: public Middleware
{
    public:
    FixedWindowLimiter(cfg::FixedWindowSetting setting): setting(setting) {clients.reserve(2048);} // avoid reshashing, could possibly add expired client removal
    FixedWindowLimiter() {clients.reserve(2048);} // avoid reshashing, could possibly add expired client removal
    asio::awaitable<void> process(Transaction* txn, Next next) override;
    private:
    std::unordered_map<std::string, std::unique_ptr<IpInfo>> clients;
    cfg::FixedWindowSetting setting;
    std::mutex clients_mutex;

    private:
    IpInfo* findClient(const std::string& ip);
};

struct Bucket {
    /* upper 32 bits = tokens, lower 32 bits = refill timestamp */
    std::atomic<std::uint64_t> tokens_and_refill;
};

class TokenBucketLimiter: public Middleware
{
    public:
    TokenBucketLimiter(cfg::TokenBucketSetting&& setting): setting(setting) {buckets.reserve(2048);}
    asio::awaitable<void> process(Transaction* txn, Next next) override;

    private:
    cfg::TokenBucketSetting setting;
    std::unordered_map<std::string, std::unique_ptr<Bucket>> buckets;
    std::mutex buckets_mutex;

    private:
    Bucket* findBucket(const std::string& identifier);
};
};
#endif