
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
    void start();
    void begin();
    void readRequest();
    
    Socket* getSocket() {return sock.get();}

    void onError();
    void onCompletion();
    void setHandler(std::unique_ptr<RequestHandler>&& handler);

    private:
    std::unique_ptr<RequestHandler> handler;
    std::unique_ptr<Socket> sock;
};

#endif
