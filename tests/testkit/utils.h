#pragma once

#include <cstddef>

#include <asio.hpp>

namespace testkit {

    inline uint16_t get_free_loopback_port() {
        asio::io_context io;
        asio::ip::tcp::acceptor acceptor(io, asio::ip::tcp::endpoint(asio::ip::address_v4::loopback(), 0));
        return acceptor.local_endpoint().port();
    }

    inline asio::ip::tcp::endpoint get_loopback_endpoint() {
        return asio::ip::tcp::endpoint(asio::ip::address_v4::loopback(), get_free_loopback_port());
    }

    inline bool wait_until_predicate(const std::function<bool()>& predicate, std::chrono::milliseconds timeout) {
        using Clock = std::chrono::steady_clock;

        const auto deadline = Clock::now() + timeout;
        while (Clock::now() < deadline) {
            if (predicate()) {
                return true;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        return predicate();
    }
}