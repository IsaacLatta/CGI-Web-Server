#pragma once

#include <atomic>
#include <mutex>
#include <string>

#include "http/mw/Middleware.h"

namespace mw {

class RateLimiter: public Middleware {
public:
    asio::awaitable<void> Process(Transaction& txn, Next next) override;
};

struct IpInfo {
    /* upper 32 bits window, lower 32 bits counter */
    std::atomic<std::uint64_t> window_and_count;
};

class FixedWindowLimiter: public Middleware {
public:
    FixedWindowLimiter(cfg::FixedWindowSetting setting): setting(setting) { clients.reserve(2048); }

    FixedWindowLimiter() { clients.reserve(2048); }

    asio::awaitable<void> Process(Transaction&, Next) override;

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

class TokenBucketLimiter: public Middleware {
public:
    TokenBucketLimiter(cfg::TokenBucketSetting&& setting): setting(setting) {buckets.reserve(2048);}
    asio::awaitable<void> Process(Transaction&, Next) override;

private:
    cfg::TokenBucketSetting setting;
    std::unordered_map<std::string, std::unique_ptr<Bucket>> buckets;
    std::mutex buckets_mutex;

private:
    Bucket* findBucket(const std::string& identifier);
};

}