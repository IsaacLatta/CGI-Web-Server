#ifndef TRANSACTION_H
#define TRANSACTION_H

#include "http.h"
#include "logger.h"
#include <vector>

class Session;
class Socket;

struct Transaction {
    std::weak_ptr<Session> session;
    Socket* sock;
    std::vector<char> buffer;
    http::Request request;
    http::Response response;
    logger::Entry log_entry;
    unsigned long bytes = 0;

    Transaction(std::weak_ptr<Session> session, Socket* sock, std::vector<char>&& buffer)
        : session(std::move(session)), sock(sock), buffer(std::move(buffer)) {}

    Transaction(std::weak_ptr<Session> session, Socket* sock)
        : session(std::move(session)), sock(sock) {}

    void addBytes(long additional_bytes) { bytes += additional_bytes; }
    void setBuffer(std::vector<char>&& new_buffer) { buffer = std::move(new_buffer); }
    void setRequest(http::Request&& new_request) {request = std::move(new_request);}

    unsigned long getBytes() const {return bytes;}
    const std::vector<char>* getBuffer() const {return &buffer;}
    http::Response* getResponse() {return &response;}
    logger::Entry* getLogEntry() {return &log_entry;}
    Socket* getSocket() {return sock;}
    http::Request* getRequest() {return &request;}
};

#endif
