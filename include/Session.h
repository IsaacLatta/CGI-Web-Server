
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
#include "Middleware.h"

class Session : public std::enable_shared_from_this<Session>
{
    public:
    Session(std::unique_ptr<Socket>&& sock) : sock(std::move(sock)){};
    asio::awaitable<void> start();
    void onError(http::error&& ec);
    
    Socket* getSocket() const {return sock.get();}

    private:
    void buildPipeline();
    asio::awaitable<void> runPipeline(Transaction* txn, std::size_t index);

    private:
    std::vector<std::unique_ptr<Middleware>> pipeline;
    std::unique_ptr<Socket> sock;
};

#endif
