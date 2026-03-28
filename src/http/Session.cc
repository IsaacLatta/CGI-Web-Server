#include "http/Session.h"

namespace http {
    asio::awaitable<void> DefaultSession::Start() {
        sock_->storeIP();
        if (asio::error_code ec = co_await sock_->co_handshake()) {
            DEBUG("Session", "handshake failed, error=%d %s", ec.value(), ec.message().c_str());
            co_return;
        }

        Transaction txn(sock_.get());
        auto pipeline = cfg::Config::getInstance()->getPipeline();
        co_await pipeline->run(&txn);
        sock_->close();
    }
}
