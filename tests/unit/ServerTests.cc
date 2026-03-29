#include <gtest/gtest.h>

#include <asio.hpp>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <thread>
#include <vector>

#include "http/Server.h"
#include "http/Session.h"

#include "io/Acceptor.h"

#include "testkit/ClientSocket.h"
#include "testkit/utils.h"
#include "testkit/fakes/fakes.h"

namespace {

class ServerTests : public ::testing::Test {
public:
    void StartServer(size_t n_threads = 2) {
        ASSERT_EQ(server_, nullptr);

        endpoint_ = testkit::get_loopback_endpoint();

        auto acceptor = std::make_unique<io::PlainAcceptor>(io_context_, endpoint_);

        http::SessionFactory factory =[this](io::SocketPtr&&) -> std::shared_ptr<http::Session> {
                return std::make_shared<testkit::fakes::CountingSession>(session_starts_);
            };

        server_ = std::make_unique<http::Server>(io_context_, std::move(acceptor), std::move(factory));

        server_thread_ = std::jthread([this, n_threads]() {
                server_->RunAndBlock(n_threads);
            });

        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    void TearDown() override {
        if (server_) {
            server_->SignalStop();
        }

        if (server_thread_.joinable()) {
            server_thread_.join();
        }

        server_.reset();
    }

protected:
    std::unique_ptr<http::Server> server_;
    asio::io_context io_context_;
    std::jthread server_thread_;

    asio::ip::tcp::endpoint endpoint_;

    std::atomic<uint64_t> session_starts_ { 0u };
};

TEST_F(ServerTests, SingleClientConnectionCreatesOneSession) {
    StartServer();

    testkit::ClientSocket client;

    auto ec = client.Connect(endpoint_, std::chrono::milliseconds(1000));
    ASSERT_FALSE(ec) << ec.message();

    EXPECT_TRUE(testkit::wait_until_predicate(
        [this]() { return session_starts_.load() >= 1; },
        std::chrono::milliseconds(1000)));

    EXPECT_EQ(session_starts_.load(), 1);
}

}  // namespace