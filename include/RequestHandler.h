#ifndef REQUESTHANDLER_H
#define REQUESTHANDLER_H

#include <vector>
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
#include "http.h"

/* Estimated BDP for typical network conditions, e.g.) RTT=20 ms, BW=100-200 Mbps*/
#define BUFFER_SIZE 262144
#define HEADER_SIZE 8192
#define DEFAULT_EXPIRATION std::chrono::system_clock::now() + std::chrono::hours{1}

class Session;

struct TransferState {
    long total_bytes{0};         
    long bytes_sent{0};          
    long current_offset{0};      
    int retry_count{1};
    static constexpr int MAX_RETRIES = 3;
    static constexpr auto RETRY_DELAY = std::chrono::milliseconds(100);
};

class RequestHandler
{
    public:
    RequestHandler(std::weak_ptr<Session> session, Socket* sock) : session(session), sock(sock), config(cfg::Config::getInstance()) {};
    virtual ~RequestHandler() = default;

    static std::unique_ptr<RequestHandler> handlerFactory(std::weak_ptr<Session>, const char* buffer, std::size_t size);

    virtual asio::awaitable<void> handle() = 0;

    protected:
    bool authenticate(cfg::Route* route);

    protected:
    std::weak_ptr<Session> session;
    Socket* sock;
    const cfg::Config* config;
};

class GetHandler: public RequestHandler
{
    public:
    GetHandler(std::weak_ptr<Session> session, Socket* sock, const char* buffer, std::size_t buf_size): 
    RequestHandler(session, sock), buffer(buffer, buffer + buf_size) {};
    
    asio::awaitable<void> handle() override;
    
    private:
    std::string buildHeader(int filefd, const std::string& content_type, long& file_len);
    asio::awaitable<void> sendResource(int filefd, long file_len);

    private:
    std::string response_header;
    std::vector<char> buffer;
};

class HeadHandler: public RequestHandler
{
    public:
    HeadHandler(std::weak_ptr<Session> session, Socket* sock, const char* buffer, std::size_t buf_size): 
    RequestHandler(session, sock), buffer(buffer, buffer + buf_size) {};
    
    asio::awaitable<void> handle() override;

    private:
    std::string buildHeader(int filefd, const std::string& content_type, long& file_len);

    private:
    std::vector<char> buffer;
};

class PostHandler: public RequestHandler 
{
    public:
    PostHandler(std::weak_ptr<Session> session, Socket* sock, const char* buffer, std::size_t buf_size): 
                RequestHandler(session, sock), buffer(buffer, buffer + buf_size) {total_bytes = 0;};
    
    asio::awaitable<void> handle() override;

    private:
    asio::awaitable<std::optional<http::error>> sendResponse(asio::posix::stream_descriptor& reader);
    std::optional<asio::posix::stream_descriptor> runScript(const http::json& args, int* pid, int* status, std::string& opt_error_msg);
    bool runProcess(int* stdin_pipe, int* stdout_pipe, pid_t* pid, int* status);
    bool parseRequest(http::json& args);
    void handleEmptyScript(std::shared_ptr<Session>);

    private:
    long total_bytes;
    std::string response_header;
    const cfg::Route* active_route;
    std::vector<char> buffer;
};

#endif