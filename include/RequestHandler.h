#ifndef REQUESTHANDLER_H
#define REQUESTHANDLER_H

#include <vector>
#include <memory>

#include "Socket.h"

/* Estimated BDP for typical network conditions, e.g.) RTT=20 ms, BW=100-200 Mbps*/
#define BUFFER_SIZE 262144
#define HEADER_SIZE 8192

class Session;

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


#endif