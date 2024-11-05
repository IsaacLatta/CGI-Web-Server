#include "Session.h"

asio::awaitable<void> Session::readRequest() {
    auto self = shared_from_this();
    std::array<char, BUFFER_SIZE> buffer;

    auto [ec, bytes_read] = co_await sock->co_read(buffer.data(), buffer.size());
    if(ec) {
        ERROR("co_read", ec.value(), ec.message().c_str() , "failed to read header");
        co_return;
    }
    
    setHandler(RequestHandler::handlerFactory(self, buffer.data(), buffer.size()));
    co_await handler->handle();
}

asio::awaitable<void> Session::start() {
    auto self = shared_from_this();
    LOG("INFO", "session", "starting handshake", "");
    asio::error_code ec = co_await sock->co_handshake();
    LOG("INFO", "session", "handshake completed", "");
    if(ec) {
        ERROR("co_handshake", ec.value(), ec.message().c_str() , "session failed to start");
        co_return;
    }
    co_await readRequest();
}

void Session::setHandler(std::unique_ptr<RequestHandler>&& handler) {
    if(!handler) {
        onError(); // example to handle the error
    }
    
    this->handler = std::move(handler);
}

void Session::begin() {
    // Start anything else, ex.) latency/RTT calc, logging etc.
    
}

/*Maybe should take a custom exception or error type as a param, depends if the request handler or session should tell the client about the error*/
void Session::onError() {
    // log error
    // could potentially invoke a retry, or just close the connection
}

void Session::onCompletion() {
    // finish any other metric calcs
    // log session
    sock->close();
}