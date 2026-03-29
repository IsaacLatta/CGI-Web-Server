#pragma once

#include <asio.hpp>
#include <asio/awaitable.hpp>

#include <chrono>
#include <span>
#include <string>

#include "io/Socket.h"

namespace testkit {

class ClientSocket final : public io::Socket {
public:
    ClientSocket() : socket_(io_context_) {}

    ~ClientSocket() override {
        Close();
    }

    asio::error_code Connect(const asio::ip::tcp::endpoint& endpoint, std::chrono::milliseconds timeout) {
        asio::error_code connect_ec;
        bool timed_out = false;

        asio::steady_timer timer(io_context_);
        timer.expires_after(timeout);

        socket_.async_connect(endpoint, [&](const asio::error_code& ec) {
            if (!timed_out && ec != asio::error::operation_aborted) {
                connect_ec = ec;
            }
            asio::error_code ignored;
            timer.cancel(ignored);
        });

        timer.async_wait([&](const asio::error_code& ec) {
            if (!ec) {
                timed_out = true;
                connect_ec = asio::error::timed_out;

                asio::error_code ignored;
                socket_.cancel(ignored);
                socket_.close(ignored);
            }
        });

        io_context_.restart();
        io_context_.run();

        return connect_ec;
    }

    void Close() {
        if (socket_.is_open()) {
            asio::error_code ignored;
            socket_.shutdown(asio::ip::tcp::socket::shutdown_both, ignored);
            socket_.close(ignored);
        }
    }

    asio::awaitable<Result> Read(std::span<char> buffer) override {
        asio::error_code ec;
        const size_t bytes = co_await socket_.async_read_some(asio::buffer(buffer.data(), buffer.size()),
                asio::redirect_error(asio::use_awaitable, ec));
        co_return Result{ ec, bytes };
    }

    asio::awaitable<Result> Write(std::span<const char> buffer) override {
        asio::error_code ec;
        const size_t bytes = co_await socket_.async_write_some(asio::buffer(buffer.data(), buffer.size()),
            asio::redirect_error(asio::use_awaitable, ec));
        co_return Result{ ec, bytes };
    }

    asio::ip::tcp::socket& GetRawSocket() override {
        return socket_;
    }

    std::string IpPortStr() const override {
        asio::error_code ec;
        const auto remote = socket_.remote_endpoint(ec);
        if (ec) {
            return "";
        }
        return remote.address().to_string() + ":" + std::to_string(remote.port());
    }

private:
    asio::io_context io_context_;
    asio::ip::tcp::socket socket_;
};

}  // namespace testkit