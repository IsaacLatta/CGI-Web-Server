#include "Session.h"

asio::awaitable<void> Session::start() {
    auto self = shared_from_this();
    
    asio::error_code error = co_await sock->co_handshake();
    if(error) {
        ERROR("co_handshake", error.value(), error.message().c_str() , "session failed to start");
        co_return;
    }

    std::array<char, BUFFER_SIZE> buffer;

    auto [ec, bytes_read] = co_await sock->co_read(buffer.data(), buffer.size());
    if(ec) {
        ERROR("co_read", ec.value(), ec.message().c_str() , "failed to read header");
        co_return;
    }
    
    handler = RequestHandler::handlerFactory(weak_from_this(), buffer.data(), buffer.size());
    if(!handler) {
        co_return;
    }
    begin();
    co_await handler->handle();
}

void Session::begin() {
    // Start anything else, ex.) latency/RTT calc, logging etc.
    
}

/*Maybe should take a custom exception or error type as a param, depends if the request handler or session should tell the client about the error*/
void Session::onError(http::error&& ec) {
    // log error
    // could potentially invoke a retry, or just close the connection
    ERROR("onError", ec.error_code, ec.message.c_str(), "invoked for client: %s", sock->getIP().c_str());


    sock->close();
}

void Session::onCompletion() {
    // finish any other metric calcs
    // log session
    sock->close();
}