#include "Session.h"

asio::awaitable<void> Session::start() {
    sock->storeIP();
    asio::error_code error = co_await sock->co_handshake();
    if (error) {
        DEBUG("Session", "handshake failed, error=%d %s", error.value(), error.message().c_str());
        co_return;
    }

    Transaction txn(sock.get());
    auto pipeline = cfg::Config::getInstance()->getPipeline();
    co_await pipeline->run(&txn);
    sock->close();
}
