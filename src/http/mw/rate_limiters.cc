#include "http/mw/rate_limiters.h"
#include "http/Exception.h"

#include <atomic>
#include <mutex>

namespace mw {
    IpInfo* FixedWindowLimiter::findClient(const std::string& ip) {
        auto it = clients.find(ip);
        if(it != clients.end()) {
            return it->second.get();
        }

        std::lock_guard<std::mutex> lock(clients_mutex);
        it = clients.find(ip);
        if(it != clients.end()) {
            return it->second.get();
        }
        auto client_ptr = std::make_unique<IpInfo>();
        auto client_raw = client_ptr.get();
        clients.emplace(ip, std::move(client_ptr));
        return client_raw;
    }

    asio::awaitable<void> mw::FixedWindowLimiter::Process(Transaction& txn, Next next) {
        std::string key = setting.make_key(&txn);
        auto client_info = findClient(key);

        auto now = std::chrono::steady_clock::now();
        auto secs = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
        std::uint32_t this_window_id = secs / setting.window_seconds;
        std::uint64_t desired;
        std::uint64_t old = client_info->window_and_count.load(std::memory_order_relaxed);
        do {
            std::uint32_t old_window_id = std::uint32_t(old >> 32);
            std::uint32_t old_count = std::uint32_t(old);

            if(old_window_id == this_window_id) {
                if(old_count >= setting.max_requests) {
                    std::unordered_map<std::string,std::string> headers;
                    uint32_t window_start = this_window_id * setting.window_seconds;
                    uint32_t reset_time   = window_start + setting.window_seconds;
                    uint32_t retry_after  = (reset_time > secs) ? (reset_time - secs) : 0;
                    headers["Retry-After"] = std::to_string(retry_after);
                    throw http::HTTPException(http::code::Too_Many_Requests,
                    std::format("client={} has exceeded {} requests in {}s", key, setting.max_requests, setting.window_seconds), std::move(headers));
                }
                desired = (std::uint64_t(this_window_id) << 32) | (old_count + 1); // increment by 1
            } else {
                desired = (std::uint64_t(this_window_id) << 32) | 1u; // reset to 1
            }
        } while(client_info->window_and_count.compare_exchange_weak(old, desired, std::memory_order_relaxed, std::memory_order_relaxed));

        std::uint32_t new_count = std::uint32_t(desired);
        std::uint32_t window_start = this_window_id * setting.window_seconds;
        std::uint32_t reset_time   = window_start + setting.window_seconds;
        auto resp = txn.getResponse();
        resp->addHeader("X-RateLimit-Limit",     std::to_string(setting.max_requests));
        resp->addHeader("X-RateLimit-Remaining", std::to_string(setting.max_requests - new_count));
        resp->addHeader("X-RateLimit-Reset",     std::to_string(reset_time));

        if(next) {
            co_await next();
        }
        co_return;
    }

    Bucket* mw::TokenBucketLimiter::findBucket(const std::string& key) {
        auto it = buckets.find(key);
        if(it != buckets.end()) {
            return it->second.get();
        }

        std::lock_guard lock(buckets_mutex);
        it = buckets.find(key);
        if(it != buckets.end()) {
            return it->second.get();
        }

        auto new_bucket = std::make_unique<Bucket>();

        auto now_s = std::uint32_t(std::chrono::duration_cast<std::chrono::seconds>
                    (std::chrono::steady_clock::now().time_since_epoch()).count());
        std::uint64_t init = (std::uint64_t(setting.capacity) << 32) | now_s;
        new_bucket->tokens_and_refill.store(init, std::memory_order_relaxed);

        auto raw = new_bucket.get();
        buckets.emplace(key, std::move(new_bucket));
        return raw;
    }

    asio::awaitable<void> mw::TokenBucketLimiter::Process(Transaction& txn, Next next) {
        std::string key = setting.make_key(&txn);
        auto bucket = findBucket(key);

        auto now = std::chrono::steady_clock::now();
        auto secs = std::uint32_t(std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count());

        std::uint32_t updated_tokens, new_refill;
        std::uint64_t desired;
        std::uint64_t old = bucket->tokens_and_refill.load(std::memory_order_relaxed);
        do {
            std::uint32_t old_tokens = std::uint32_t(old >> 32); // extract upper 32
            std::uint32_t old_refill = std::uint32_t(old); // extract lower 32
            std::uint32_t elapsed = secs > old_refill ? secs - old_refill : 0; // check on multiple req/s + overflow on secs
            std::uint64_t new_tokens = old_tokens + setting.refill_rate*elapsed;
            if(new_tokens > setting.capacity) {
                new_tokens = setting.capacity;
            }

            new_refill = elapsed > 0 ? secs : old_refill; // has a second passed ?

            if(new_tokens < 1) {
                std::unordered_map<std::string, std::string> headers;
                std::uint32_t retry_after = new_refill + 1 > secs ? (new_refill + 1 - secs) : 0;
                headers["Retry-After"] = std::to_string(retry_after);
                throw http::HTTPException(http::code::Too_Many_Requests,
                    std::format("client={} has exceeded rate limit on [{} {}] ({} tokens/s cap={} tokens)",
                    txn.getSocket()->IpPortStr(), http::method_enum_to_str(txn.getRequest()->method),
                    txn.getRequest()->endpoint_url, setting.refill_rate, setting.capacity), std::move(headers));
            }

            updated_tokens = std::uint32_t(new_tokens - 1);
            desired = (std::uint64_t(updated_tokens) << 32) | new_refill;
        } while(!bucket->tokens_and_refill.compare_exchange_weak(old, desired, std::memory_order_relaxed, std::memory_order_relaxed));

        std::uint32_t delay = (new_refill + 1 > secs) ? (new_refill + 1 - secs) : 0;
        auto system_now = std::chrono::system_clock::now();
        auto reset_tp   = system_now + std::chrono::seconds(delay);
        auto reset_epoch = std::chrono::duration_cast<std::chrono::seconds>(reset_tp.time_since_epoch()).count();
        auto response = txn.getResponse();
        response->addHeader("X-RateLimit-Limit", std::to_string(setting.capacity));
        response->addHeader("X-RateLimit-Remaining", std::to_string(updated_tokens));
        response->addHeader("X-RateLimit-Reset", std::to_string(reset_epoch));
        response->addHeader("Retry-After", std::to_string(delay));

        if(next) {
            co_await next();
        }
        co_return;
    }

    asio::awaitable<void> RateLimiter::Process(Transaction& txn, Next next) {
        auto limiter = txn.getRequest()->route->rate_limiter.get();
        if(limiter) {
            co_await limiter->Process(txn, next);
        } else {
            co_await next();
        }
        co_return;
    }
}
