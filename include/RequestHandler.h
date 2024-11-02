#ifndef REQUESTHANDLER_H
#define REQUESTHANDLER_H

#include <vector>
#include <memory>

#include "Socket.h"

/* Estimated BDP for typical network conditions, e.g.) RTT=20 ms, BW=100-200 Mbps*/
#define BUFFER_SIZE 262144

class Session;

class RequestHandler
{
    public:
    RequestHandler(std::shared_ptr<Session>, Socket* sock) : sock(sock) {};
    virtual ~RequestHandler() = default;

    static std::unique_ptr<RequestHandler> handlerFactory(std::shared_ptr<Session>, std::shared_ptr<std::vector<char>> buffer);

    virtual void handle() = 0;

    protected:
    std::shared_ptr<Session> session;
    Socket* sock;
};

class GetHandler: public RequestHandler
{
    public:
    GetHandler(std::shared_ptr<Session> session, Socket* sock, std::shared_ptr<std::vector<char>> buffer): RequestHandler(session, sock), buffer(buffer) {};
    void handle() override;
    private:
    std::shared_ptr<std::vector<char>> buffer;
};


#endif