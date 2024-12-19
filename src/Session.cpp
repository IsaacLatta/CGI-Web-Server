#include "Session.h"

asio::awaitable<void> Session::start() {
    auto self = shared_from_this();
    
    sock->storeIP();
    asio::error_code error = co_await sock->co_handshake();
    if(error) {
        ERROR("co_handshake", error.value(), error.message().c_str() , "session failed to start");
        co_return;
    }
    
    std::vector<char> buffer;
    auto [ec, bytes_read] = co_await sock->co_read(buffer.data(), buffer.size());
    if(ec) {
        ERROR("co_read", error.value(), error.message().c_str(), "failed to read request headr");
        co_return;
    }

    Transaction txn(weak_from_this(), sock.get(), std::move(buffer));
    buildPipeline();
    co_await runPipeline(&txn, 0); // call stack builds here
    sock->close();
}

/* Add authentication later */
void Session::buildPipeline() {
    pipeline.push_back(std::make_unique<ErrorHandlerMiddleware>());
    pipeline.push_back(std::make_unique<LoggingMiddleware>());
    pipeline.push_back(std::make_unique<RequestHandlerMiddleware>());
}

asio::awaitable<void> Session::runPipeline(Transaction* txn, std::size_t index) {
    if (index >= pipeline.size()) {
        co_return;
    }

    Middleware* mw = pipeline[index].get();
    Next next_func = [this, txn, index] () -> asio::awaitable<void> {
        co_await runPipeline(txn, index + 1);
    };
    mw->process(txn, next_func);
}

void Session::onError(http::error&& ec) {
    char buffer[BUFSIZ];
    snprintf(buffer, BUFSIZ, "Invoked for client %s", sock->getIP().c_str());
    ERROR(buffer, static_cast<int>(ec.error_code), ec.message.c_str(), "");
    
//    session_info.end_time = std::chrono::system_clock::now();
//    session_info.response = ec.response;
//    logger::log_session(session_info, "ERROR");

    if(ec.error_code != http::code::Client_Closed_Request) {
        sock->write(ec.response.data(), ec.response.length());
    }
    sock->close();
}
