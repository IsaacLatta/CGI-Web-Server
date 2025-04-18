#include "Session.h"

asio::awaitable<void> Session::start() {
    sock->storeIP();
    asio::error_code error = co_await sock->co_handshake();
    if (error) {
        DEBUG("Session", "handshake failed, error=%d %s", error.value(), error.message().c_str());
        co_return;
    }

    Transaction txn(sock.get());
    buildPipeline();
    co_await runPipeline(&txn, 0);
    sock->close();
}

void Session::buildPipeline() {
    pipeline.push_back(std::make_unique<mw::Logger>());
    pipeline.push_back(std::make_unique<mw::ErrorHandler>());
    pipeline.push_back(std::make_unique<mw::Parser>());
    pipeline.push_back(std::make_unique<mw::Authenticator>());
}

asio::awaitable<void> Session::runPipeline(Transaction *txn, std::size_t index)
{
    if (index >= pipeline.size()) {
        co_return;
    }

    mw::Middleware *mw = pipeline[index].get();
    mw::Next next_func = [this, txn, index]() -> asio::awaitable<void> {
        co_await runPipeline(txn, index + 1);
    };
    co_await mw->process(txn, next_func);
}
