
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
#include "config.h"

class Session : public std::enable_shared_from_this<Session>
{
    public:
    Session(std::unique_ptr<Socket>&& sock, std::unordered_map<std::string, config::Route>& routes) : sock(std::move(sock)), routes(routes) {};
    asio::awaitable<void> start();
    void begin(const char* request_header, std::size_t header_size);
    void onError(http::error&& ec);
    void onCompletion(const std::string& response, long bytes_moved);
    
    Socket* getSocket() const {return sock.get();}
    std::unordered_map<config::Endpoint, config::Route>& getRoutes() {return routes;}

    private:
    logger::entry session_info;
    std::unordered_map<config::Endpoint, config::Route>& routes;
    std::unique_ptr<RequestHandler> handler;
    std::unique_ptr<Socket> sock;
};

#endif
