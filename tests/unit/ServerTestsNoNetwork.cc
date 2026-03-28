#include <gtest/gtest.h>

#include <asio.hpp>
#include <asio/awaitable.hpp>
#include <asio/use_awaitable.hpp>

#include <atomic>
#include <memory>
#include <optional>
#include <queue>
#include <span>
#include <stdexcept>
#include <thread>
#include <utility>
#include <vector>

#include "http/Server.h"
#include "http/Session.h"
#include "io/Acceptor.h"
#include "io/Socket.h"

#include "fakes/fakes.h"

namespace {

class FakeSession final : public http::Session {
public:
    explicit FakeSession(std::atomic<uint64_t>& starts) : starts_(starts) {}

    asio::awaitable<void> Start() override {
        ++starts_;
        co_return;
    }

private:
    std::atomic<uint64_t>& starts_;
};

class FakeAcceptor final : public io::Acceptor {
public:
    explicit FakeAcceptor(std::vector<io::AcceptResult>&& results)
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

TEST(Server, RunAndBlockThrowsOnZeroThreads) {
    asio::io_context io;

    auto acceptor = std::make_unique<FakeAcceptor>(
        std::vector<io::AcceptResult>{});

    http::SessionFactory factory = [](io::SocketPtr) -> std::shared_ptr<http::Session> {
        throw std::logic_error("session factory should not be called");
    };

    http::Server server(io, std::move(acceptor), factory);

    EXPECT_THROW(server.RunAndBlock(0), std::invalid_argument);
}


TEST(Server, OperationAbortedAcceptStopsServerCleanly) {
    asio::io_context io;

    std::vector<io::AcceptResult> results;
    results.push_back(io::AcceptResult{
            asio::error::make_error_code(asio::error::operation_aborted),
            nullptr});

    auto acceptor = std::make_unique<FakeAcceptor>(std::move(results));

    std::atomic<uint64_t> session_starts { 0 };
    http::SessionFactory factory = [&session_starts](io::SocketPtr&&) mutable {
        return std::make_shared<FakeSession>(session_starts);
    };

    http::Server server(io, std::move(acceptor), factory);

    EXPECT_NO_THROW(server.RunAndBlock(1));
    EXPECT_EQ(session_starts.load(), 0);
}

TEST(Server, SuccessfulAcceptCreatesAndStartsSession) {
    asio::io_context io;

    std::vector<io::AcceptResult> results;
    results.push_back({asio::error_code{}, std::make_unique<testkit::fakes::NoOpSocket>() });
    results.push_back({asio::error::make_error_code(asio::error::operation_aborted), nullptr });
    auto acceptor = std::make_unique<FakeAcceptor>(std::move(results));

    std::atomic<uint64_t> factory_calls { 0u };
    std::atomic<uint64_t> session_starts { 0u };

    http::SessionFactory factory = [&factory_calls, &session_starts](io::SocketPtr) -> std::shared_ptr<http::Session> {
            ++factory_calls;
            return std::make_shared<FakeSession>(session_starts);
        };

    http::Server server(io, std::move(acceptor), factory);

    EXPECT_NO_THROW(server.RunAndBlock(1));

    EXPECT_EQ(factory_calls.load(), 1);
    EXPECT_EQ(session_starts.load(), 1);
}

}  // namespace