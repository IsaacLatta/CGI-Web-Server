
#ifndef SESSION_H
#define SESSION_H

#include <asio.hpp>
#include <asio/ssl.hpp>
#include <vector>
#include <memory>
#include <iostream>
#include <chrono>

#include "MethodHandler.h"
#include "logger.h"
#include "http.h"
#include "config.h"
#include "Middleware.h"

class Session
{
    public:
    Session(std::unique_ptr<Socket>&& sock) : sock(std::move(sock)){};
    
    asio::awaitable<void> start();
    Socket* getSocket() const {return sock.get();}

    std::vector<std::unique_ptr<mw::Middleware>> pipeline;
    std::unique_ptr<Socket> sock;
};

#endif
