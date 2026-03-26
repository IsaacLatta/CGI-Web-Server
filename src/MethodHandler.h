#ifndef METHODHANDLER_H
#define METHODHANDLER_H

#include <vector>
#include <span>
#include <memory>
#include <unistd.h>
#include <asio.hpp>
#include <asio/steady_timer.hpp>
#include <unordered_map> 
#include <sys/wait.h>
#include <spawn.h>
#include <optional>
#include <jwt-cpp/jwt.h>

#include "Socket.h"
#include "config.h"
#include "Transaction.h"
#include "Streamer.h"

#define DEFAULT_EXPIRATION std::chrono::system_clock::now() + std::chrono::hours{1}

class MethodHandler : public std::enable_shared_from_this<MethodHandler>
{
    public:
    MethodHandler(Transaction* txn) : 
                txn(txn), sock(txn->getSocket()), request(txn->getRequest()), response(txn->getResponse()), config(cfg::Config::getInstance()) {} 
    
    virtual ~MethodHandler() = default;

    virtual asio::awaitable<void> handle() = 0;

    protected:
    Transaction* txn;
    Socket* sock;
    http::Request* request;
    http::Response* response;
    const cfg::Config* config;
};

class GetHandler: public MethodHandler
{
    public:
    GetHandler(Transaction* txn): MethodHandler(txn) {};
    
    asio::awaitable<void> handle() override;

    private:
    asio::awaitable<void> handleScript();
    asio::awaitable<void> handleFile();
};

class HeadHandler: public MethodHandler
{
    public:
    HeadHandler(Transaction* txn): MethodHandler(txn) {};
    
    asio::awaitable<void> handle() override;

    private:
    void buildResponse();

    private:
};

class PostHandler: public MethodHandler 
{
    public:
    PostHandler(Transaction* txn): MethodHandler(txn) {};
    
    asio::awaitable<void> handle() override;

    private:
    long total_bytes;
    std::string response_header;
};

class OptionsHandler: public MethodHandler
{
    public:
    OptionsHandler(Transaction* txn): MethodHandler(txn) {};

    asio::awaitable<void> handle() override;
};

#endif