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

/* Estimated BDP for typical network conditions, e.g.) RTT=20 ms, BW=100-200 Mbps*/
#define BUFFER_SIZE 262144
#define HEADER_SIZE 8192
#define DEFAULT_EXPIRATION std::chrono::system_clock::now() + std::chrono::hours{1}

struct TransferState {
    long total_bytes{0};         
    long bytes_sent{0};          
    long current_offset{0};      
    int retry_count{1};
    static constexpr int MAX_RETRIES = 3;
    static constexpr auto RETRY_DELAY = std::chrono::milliseconds(100);
};

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

    asio::awaitable<void> writeHeader();
    asio::awaitable<void> writeResource(int filefd, long file_len);
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
    asio::awaitable<void> readResponse(asio::posix::stream_descriptor& reader);
    asio::posix::stream_descriptor runScript(int* pid, int* status);
    bool runProcess(int* stdin_pipe, int* stdout_pipe, pid_t* pid, int* status);

    private:
    long total_bytes;
    std::string response_header;
};


// class Options: public MethodHandler
// {
//     public:
//     asio::awaitable<void> handle() override;
// }

#endif