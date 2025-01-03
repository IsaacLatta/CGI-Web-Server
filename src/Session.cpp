#include "Session.h"

asio::awaitable<void> Session::start() {
    sock->storeIP();
    asio::error_code error = co_await sock->co_handshake();
    if(error) {
        ERROR("co_handshake", error.value(), error.message().c_str() , "session failed to start");
        co_return;
    }
    
    LOG("DEBUG", "Session", "starting pipeline for client: %s", sock->getIP().c_str());
    Transaction txn(sock.get());
    buildPipeline();
    co_await runPipeline(&txn, 0); 
    sock->close();
    LOG("DEBUG", "Session", "session ended for client: %s", sock->getIP().c_str());
}

void Session::buildPipeline() {
    pipeline.push_back(std::make_unique<mw::Logger>());
    pipeline.push_back(std::make_unique<mw::ErrorHandler>());
    pipeline.push_back(std::make_unique<mw::Parser>());
    pipeline.push_back(std::make_unique<mw::Authenticator>());
    pipeline.push_back(std::make_unique<mw::RequestHandler>());
}

asio::awaitable<void> Session::runPipeline(Transaction* txn, std::size_t index) {
    if (index >= pipeline.size()) {
        LOG("DEBUG", "Pipeline", "finished");
        co_return;
    }

    mw::Middleware* mw = pipeline[index].get();
    mw::Next next_func = [this, txn, index] () -> asio::awaitable<void> {
        co_await runPipeline(txn, index + 1);
    };
    co_await mw->process(txn, next_func);
}
