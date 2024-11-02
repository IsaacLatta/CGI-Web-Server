#include "Session.h"

void Session::readRequest() {
    auto self = shared_from_this();
    auto buffer = std::make_shared<std::vector<char>>(BUFFER_SIZE);
    
    sock->do_read(buffer->data(), buffer->size(),
    [self, buffer](const asio::error_code error, std::size_t size) {
        if(error) {
            // handle error
        }
        self->setHandler(RequestHandler::handlerFactory(self, buffer));
        self->begin();
    });
}

void Session::start()
{
    auto self = shared_from_this();
    sock->do_handshake(
        [self](const asio::error_code &error) {
            if (error) {
                // handle error;
            }
            self->readRequest();
        });
}

void Session::setHandler(std::unique_ptr<RequestHandler>&& handler) {
    if(!handler) {
        onError(); // example to handle the error
    }
    
    this->handler = std::move(handler);
}

void Session::begin() {
    // Start anything else, ex.) latency/RTT calc, logging etc.
    
    handler->handle();
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