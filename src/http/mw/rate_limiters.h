#pragma once

#include <atomic>
#include <mutex>
#include <string>

#include "Context.h"
#include "http/mw/Middleware.h"
#include "config/config.h"

namespace mw {

class RateLimiter: public Middleware<http::PostRouteContext> {
public:
    asio::awaitable<void> Process(http::PostRouteContext&, NextCallback, FinishCallback) override;
};

struct IpInfo {
    /* upper 32 bits window, lower 32 bits counter */
    std::atomic<std::uint64_t> window_and_count;
};

class FixedWindowLimiter: public Middleware<http::PostRouteContext> {
public:
    FixedWindowLimiter(cfg::FixedWindowSetting setting): setting(setting) { clients.reserve(2048); }

    FixedWindowLimiter() { clients.reserve(2048); }

    asio::awaitable<void> Process(http::PostRouteContext&, NextCallback, FinishCallback) override;

private:
    std::unordered_map<std::string, std::unique_ptr<IpInfo>> clients;
    cfg::FixedWindowSetting setting;
    std::mutex clients_mutex;

private:
    IpInfo* findClient(const std::string& ip);
};

struct Bucket {
    /* upper 32 bits = tokens, lower 32 bits = refill timestamp */
    std::atomic<uint64_t> tokens_and_refill;
};

class TokenBucketLimiter: public Middleware<http::PostRouteContext> {
public:
    TokenBucketLimiter(cfg::TokenBucketSetting&& setting): setting(setting) {buckets.reserve(2048);}
    asio::awaitable<void> Process(http::PostRouteContext&, NextCallback, FinishCallback) override;

private:
    cfg::TokenBucketSetting setting;
    std::unordered_map<std::string, std::unique_ptr<Bucket>> buckets;
    std::mutex buckets_mutex;

private:
    Bucket* findBucket(const std::string& identifier);
};

}