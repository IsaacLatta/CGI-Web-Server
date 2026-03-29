#pragma once

#include "io/Acceptor.h"
#include "io/Socket.h"
#include "http/Session.h"

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

class CountingSession final : public http::Session {
public:
    explicit CountingSession(std::atomic<uint64_t>& starts)
        : starts_(starts) {}

    asio::awaitable<void> Start() override {
        ++starts_;
        co_return;
    }

private:
    std::atomic<uint64_t>& starts_;
};

class ProgrammedAcceptor final : public io::Acceptor {
public:
    explicit ProgrammedAcceptor(std::vector<io::AcceptResult>&& results)
        : results_(std::move(results)) {}

    void Close() override {
        closed_ = true;
    }

    asio::awaitable<io::AcceptResult> Accept() override {
        if (index_ >= results_.size()) {
            co_return io::AcceptResult{asio::error::operation_aborted, nullptr};
        }

        co_return std::move(results_[index_++]);
    }

private:
    bool closed_ { false };
    std::vector<io::AcceptResult> results_;
    size_t index_ { 0u };
};

}
