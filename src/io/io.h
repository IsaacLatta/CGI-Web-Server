#pragma once

#include <span>

#include <asio.hpp>

#include "core/time.h"

#include "io/forward.h"
#include "io/Socket.h"

namespace io {

    struct TransferState {
        static constexpr uint32_t MAX_RETRIES { 3u };
        static constexpr auto RETRY_DELAY { core::Ms(100u) };

        size_t TotalBytes { 0u };
        size_t BytesSent { 0u };
        size_t CurrentOffset { 0u };
        uint32_t RetryCount { 1u };
    };

    asio::awaitable<void> do_backoff(const asio::error_code& ec, uint32_t retry_idx);

    asio::awaitable<Socket::Result> co_write_all(Socket&, std::span<const char>);

    inline bool is_client_disconnect(const asio::error_code& ec) noexcept {
        return ec == asio::error::connection_reset || ec == asio::error::broken_pipe || ec == asio::error::eof;
    }

    inline bool is_permanent_failure(const asio::error_code& ec) noexcept {
        return ec == asio::error::bad_descriptor || ec == asio::error::address_in_use;
    }

    inline bool is_retryable(const asio::error_code& ec) noexcept {
        return ec == asio::error::would_block || ec == asio::error::try_again || ec == asio::error::network_unreachable
            || ec == asio::error::host_unreachable || ec == asio::error::connection_refused || ec == asio::error::timed_out
            || ec == asio::error::no_buffer_space;
    }
}