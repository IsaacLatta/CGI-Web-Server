#pragma once

#include "io/Acceptor.h"
#include "io/Socket.h"

namespace testkit::fakes {

    class NoOpSocket final : public io::Socket {
    public:
        asio::awaitable<Result> Read(std::span<char>) override {
            co_return Result{};
        }

        asio::awaitable<Result> Write(std::span<const char>) override {
            co_return Result{};
        }

        asio::ip::tcp::socket& GetRawSocket() override {
            return socket_;
        }

    private:
        asio::io_context ctx_;
        asio::ip::tcp::socket socket_ { ctx_ };
    };

}
