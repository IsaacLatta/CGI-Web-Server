
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
    void begin(const char* request_header, std::size_t header_size);
    
    Socket* getSocket() const {return sock.get();}

    void onError(http::error&& ec);
    void onCompletion(const std::string& response, long bytes_moved);

    private:
    logger::entry session_info;
    std::unique_ptr<RequestHandler> handler;
    std::unique_ptr<Socket> sock;
};

#endif
