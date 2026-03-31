#include "Server.h"

#include <asio/co_spawn.hpp>
#include <asio/detached.hpp>
#include <asio/use_awaitable.hpp>

#include "io/io.h"
#include "io/Acceptor.h"

#include "logger/macros.h"
#include "parsing/parse.h"

#include "http/Session.h"


namespace {

    bool is_fatal(asio::error_code ec) {
        return ec.value() == asio::error::bad_descriptor ||
            ec.value() == asio::error::access_denied ||
            ec.value() == asio::error::address_in_use;
    }

}

namespace http {

Server::Server(asio::io_context& io_context, io::AcceptorPtr acceptor, SessionFactory session_factory)
    :   io_context_(io_context), acceptor_(std::move(acceptor)), session_factory_(std::move(session_factory)) {}

asio::awaitable<void> Server::AcceptLoop() {
    while(true) {
        auto [ec, socket] = co_await acceptor_->Accept();
        if (ShouldExit(ec)) {
            co_return;
        }

        if (ec) {
            continue;
        }

        const auto session = session_factory_(std::move(socket));
        asio::co_spawn(io_context_, session->Start(), asio::detached);
    }
}

void Server::Stop() {
    acceptor_->Close();

    if (!io_context_.stopped()) {
        io_context_.stop();
    }

    for (auto& thread: threads_) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    threads_.clear();
}

void Server::SignalStop() {
    io_context_.stop();
}

void Server::RunAndBlock(size_t n_threads) {
    if (n_threads == 0) {
        throw std::invalid_argument("cannot start server with 0 threads");
    }

    asio::co_spawn(io_context_, AcceptLoop(), asio::detached);

    threads_.reserve(n_threads);
    for(size_t i = 0; i < n_threads - 1; ++i) {
        threads_.emplace_back([this](){ io_context_.run();});
    }
    io_context_.run();

    Stop();
}

bool Server::ShouldExit(const asio::error_code& error) {
    if(!error) {
        retries_ = 0;
        return false;
    }

    if (error == asio::error::operation_aborted) {
        return true;
    }

    DEBUG("Server", "async accept: error=%d %s", error.value(), error.message().c_str());

    if(retries_ > MAX_RETRIES || is_fatal(error)) {
        FATAL("Server", "error=%d %s", error.value(), error.message().c_str());
        return true;
    }

    if(io::is_retryable(error)) {
        retries_++;
        const size_t backoff_time_ms = DEFAULT_BACKOFF_MS * retries_;
        WARN("Server", "error=%d %s, backing off for %ld ms", error.value(), error.message().c_str(), backoff_time_ms);
        std::this_thread::sleep_for(std::chrono::milliseconds(backoff_time_ms));
    }

    return false;
}
}
