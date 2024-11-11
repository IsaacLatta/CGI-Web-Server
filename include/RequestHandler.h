#ifndef REQUESTHANDLER_H
#define REQUESTHANDLER_H

#include <vector>
#include <memory>
#include <unistd.h>
#include <asio.hpp>
#include <asio/steady_timer.hpp>
#include <unordered_map> 
#include <spawn.h>
#include <optional>

#include "Socket.h"
#include "config.h"
#include "http.h"

/* Estimated BDP for typical network conditions, e.g.) RTT=20 ms, BW=100-200 Mbps*/
#define BUFFER_SIZE 262144
#define HEADER_SIZE 8192

class Session;

struct TransferState {
    long total_bytes{0};         
    long bytes_sent{0};          
    long current_offset{0};      
    int retry_count{0};
    static constexpr int MAX_RETRIES = 3;
    static constexpr auto RETRY_DELAY = std::chrono::milliseconds(100);
};

class RequestHandler
{
    public:
    RequestHandler(std::weak_ptr<Session> session, Socket* sock) : session(session), sock(sock) {};
    virtual ~RequestHandler() = default;

    static std::unique_ptr<RequestHandler> handlerFactory(std::weak_ptr<Session>, const char* buffer, std::size_t size);

    virtual asio::awaitable<void> handle() = 0;

    protected:
    std::weak_ptr<Session> session;
    Socket* sock;
};

class GetHandler: public RequestHandler
{
    public:
    GetHandler(std::weak_ptr<Session> session, Socket* sock, const char* buffer, std::size_t buf_size): RequestHandler(session, sock), buffer(buffer, buffer + buf_size) {};
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
    HeadHandler(std::weak_ptr<Session> session, Socket* sock, const char* buffer, std::size_t buf_size): RequestHandler(session, sock), buffer(buffer, buffer + buf_size) {};
    asio::awaitable<void> handle() override;

    private:
    std::string buildHeader(int filefd, const std::string& content_type, long& file_len);

    private:
    std::vector<char> buffer;
};

class PostHandler: public RequestHandler 
{
    public:
    PostHandler(std::weak_ptr<Session> session, Socket* sock, const char* buffer, std::size_t buf_size, std::unordered_map<config::Endpoint, config::Route>& routes): 
                RequestHandler(session, sock), buffer(buffer, buffer + buf_size),  routes(routes) {};
    
    asio::awaitable<void> handle() override;

    private:
    asio::awaitable<std::optional<http::error>> sendResponse(asio::posix::stream_descriptor& reader);
    std::optional<asio::posix::stream_descriptor> runScript(const http::json& args, std::string& opt_error_msg);
    bool runProcess(int* stdin_pipe, int* stdout_pipe, pid_t* pid, int* status);

    private:
    long total_bytes;
    std::string response_header;
    const config::Route* active_route;
    const std::unordered_map<config::Endpoint, config::Route>& routes;
    std::vector<char> buffer;
};

#endif