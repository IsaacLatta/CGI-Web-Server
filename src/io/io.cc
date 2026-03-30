#include "io/io.h"

#include <random>

#include "io/Socket.h"

namespace {

core::Ms select_backoff(const asio::error_code& ec, uint32_t attempt) noexcept {
    if(!ec || io::is_client_disconnect(ec) || io::is_permanent_failure(ec)) {
        return core::Ms{ 0u };
    }

    if(ec == asio::error::no_buffer_space) {
        return core::Ms{ 1000u } * attempt;
    }

    constexpr uint32_t MAX_ATTEMPTS { 6u };
    constexpr auto BASE_DELAY_MS = core::Ms{ 50u };
    constexpr auto MAX_DELAY_MS = core::Ms{ 2000u };
    static thread_local std::mt19937_64 rng(std::random_device{}());

    auto exp_delay = BASE_DELAY_MS * (1 << std::min(attempt, MAX_ATTEMPTS)); // multiply by 2^attempt
    if(exp_delay > MAX_DELAY_MS) {
        exp_delay = MAX_DELAY_MS;
    }

    const auto jitter_range = exp_delay.count() / 10; // 10% of the ticks for the delay, to avoid contention on wake-ups
    std::uniform_int_distribution<int64_t> dist(-jitter_range, jitter_range);
    return exp_delay + core::Ms{ dist(rng) };
}

}

namespace io {

asio::awaitable<Socket::Result> co_write_all(Socket& sock, std::span<const char> buffer) {
    TransferState state;
    state.BytesSent = 0;
    state.TotalBytes = buffer.size();
    while(state.BytesSent < state.TotalBytes) {
        auto [ec, bytes_written] = co_await sock.Write(buffer.subspan(state.BytesSent));

        if(is_permanent_failure(ec) || state.RetryCount > TransferState::MAX_RETRIES) {
            co_return Socket::Result{ ec, state.BytesSent };
        }

        if(is_retryable(ec)) {
            co_await do_backoff(ec, state.RetryCount);
            state.RetryCount++;
        }

        state.BytesSent += bytes_written;
    }

    co_return Socket::Result{ {}, state.BytesSent };
}

asio::awaitable<void> do_backoff(const asio::error_code& ec, uint32_t attempt) {
    const auto delay = select_backoff(ec, attempt);
    if(delay.count() <= 0) {
        co_return;
    }

    asio::steady_timer timer(co_await asio::this_coro::executor);
    timer.expires_after(delay);
    co_await timer.async_wait(asio::use_awaitable);
    co_return;
}


}