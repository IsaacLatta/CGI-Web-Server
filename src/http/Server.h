#pragma once

#include <asio.hpp>

#include "http/forward.h"

namespace http {
class Server {
public:
    static constexpr size_t DEFAULT_BACKOFF_MS { 100u };
    static constexpr size_t MAX_RETRIES { 5u };

public:
    Server(asio::io_context&, ::io::AcceptorPtr, SessionFactory);
    void RunAndBlock(size_t n_threads);
    void Stop();

private:
    asio::awaitable<void> AcceptLoop();
    bool ShouldExit(const asio::error_code& error);

private:
    asio::io_context& io_context_;

    ::io::AcceptorPtr acceptor_ { nullptr };

    SessionFactory session_factory_;

    size_t retries_ { 0u };
    std::vector<std::thread> threads_;
};

}
