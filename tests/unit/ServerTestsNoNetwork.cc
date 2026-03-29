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

#include "testkit/fakes/fakes.h"

namespace {

TEST(Server, RunAndBlockThrowsOnZeroThreads) {
    asio::io_context io;

    auto acceptor = std::make_unique<testkit::fakes::ProgrammedAcceptor>(
        std::vector<io::AcceptResult>{});

    http::SessionFactory factory = [](io::SocketPtr) -> std::shared_ptr<http::Session> {
        throw std::logic_error("session factory should not be called");
    };

    http::Server server(io, std::move(acceptor), factory);

    EXPECT_THROW(server.RunAndBlock(0u), std::invalid_argument);
}

TEST(Server, OperationAbortedAcceptStopsServerCleanly) {
    asio::io_context io;

    std::vector<io::AcceptResult> results;
    results.push_back(io::AcceptResult{
            asio::error::make_error_code(asio::error::operation_aborted),
            nullptr});

    auto acceptor = std::make_unique<testkit::fakes::ProgrammedAcceptor>(std::move(results));

    std::atomic<uint64_t> session_starts { 0u };
    http::SessionFactory factory = [&session_starts](io::SocketPtr&&) mutable {
        return std::make_shared<testkit::fakes::CountingSession>(session_starts);
    };

    http::Server server(io, std::move(acceptor), factory);

    EXPECT_NO_THROW(server.RunAndBlock(1u));
    EXPECT_EQ(session_starts.load(), 0u);
}

TEST(Server, SuccessfulAcceptCreatesAndStartsSession) {
    asio::io_context io;

    std::vector<io::AcceptResult> results;
    results.push_back({ asio::error_code{}, std::make_unique<testkit::fakes::NoOpSocket>() });
    results.push_back({ asio::error::make_error_code(asio::error::operation_aborted), nullptr });
    auto acceptor = std::make_unique<testkit::fakes::ProgrammedAcceptor>(std::move(results));

    std::atomic<uint64_t> session_starts { 0u };

    http::SessionFactory factory = [&session_starts](io::SocketPtr) -> std::shared_ptr<http::Session> {
            return std::make_shared<testkit::fakes::CountingSession>(session_starts);
        };

    http::Server server(io, std::move(acceptor), factory);

    EXPECT_NO_THROW(server.RunAndBlock(1u));

    EXPECT_EQ(session_starts.load(), 1u);
}

}  // namespace