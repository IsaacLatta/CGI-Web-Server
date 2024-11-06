
#ifndef SESSION_H
#define SESSION_H

#include <asio.hpp>
#include <asio/ssl.hpp>
#include <vector>
#include <memory>
#include <iostream>
#include <chrono>

#include "RequestHandler.h"
#include "logger.h"
#include "http.h"

class Session : public std::enable_shared_from_this<Session>
{
    public:
    Session(std::unique_ptr<Socket>&& sock) : sock(std::move(sock)) {};
    asio::awaitable<void> start();
    void begin();
    
    Socket* getSocket() {return sock.get();}

    void onError(http::error&& ec);
    void onCompletion();

    private:
    std::unique_ptr<RequestHandler> handler;
    std::unique_ptr<Socket> sock;
};

#endif
