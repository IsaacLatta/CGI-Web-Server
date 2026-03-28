#pragma once

#include <vector>

#include "http.h"

#include "io/Socket.h"

#include "logger/logger.h"

struct Transaction {
    io::Socket* sock;
    std::vector<char> buffer;
    std::function<asio::awaitable<void>()> finish;
    http::Request request;
    http::Response response;
    logger::SessionEntry log_entry;

    Transaction(io::Socket* sock): sock(sock), buffer(BUFSIZ), finish(nullptr) {}
    void addBytes(long additional_bytes) {log_entry.bytes += additional_bytes;}
    void setBuffer(std::vector<char>&& new_buffer) {buffer = std::move(new_buffer);}
    void setRequest(http::Request&& new_request) {request = std::move(new_request);}

    std::vector<char>* getBuffer() {return &buffer;}
    http::Response* getResponse() {return &response;}
    logger::SessionEntry* getLogEntry() {return &log_entry;}
    io::Socket* getSocket() {return sock;}
    http::Request* getRequest() {return &request;}
};

