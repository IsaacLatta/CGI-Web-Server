#include "Session.h"

asio::awaitable<void> Session::start() {
    auto self = shared_from_this();
    
    sock->storeIP();
    asio::error_code error = co_await sock->co_handshake();
    if(error) {
        ERROR("co_handshake", error.value(), error.message().c_str() , "session failed to start");
        co_return;
    }
    
    LOG("DEBUG", "Session", "starting pipeline for client: %s", sock->getIP().c_str());
    Transaction txn(weak_from_this(), sock.get());
    buildPipeline();
    co_await runPipeline(&txn, 0); // call stack builds here
    sock->close();
    LOG("DEBUG", "Session", "session ended for client: %s", sock->getIP().c_str());
}

void Session::buildPipeline() {
    pipeline.push_back(std::make_unique<ErrorHandlerMiddleware>());
    pipeline.push_back(std::make_unique<LoggingMiddleware>());
    pipeline.push_back(std::make_unique<ParserMiddleware>());
    pipeline.push_back(std::make_unique<AuthenticatorMiddleware>());
    pipeline.push_back(std::make_unique<RequestHandlerMiddleware>());
}

asio::awaitable<void> Session::runPipeline(Transaction* txn, std::size_t index) {
    if (index >= pipeline.size()) {
        LOG("DEBUG", "Pipeline", "finished");
        co_return;
    }

    Middleware* mw = pipeline[index].get();
    Next next_func = [this, txn, index] () -> asio::awaitable<void> {
        co_await runPipeline(txn, index + 1);
    };
    co_await mw->process(txn, next_func);
    LOG("DEBUG", "MW", "processed: %d", static_cast<int>(index));
}
