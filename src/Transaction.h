#pragma once

#include <vector>

#include "http.h"

#include "io/Socket.h"

#include "logger/logger.h"
#include "http/Response.h"
#include "http/Request.h"

struct Transaction {

    Transaction(io::SocketPtr&& sock): socket_(std::move(sock)), buffer(BUFSIZ), finish(nullptr) {}

    void addBytes(long additional_bytes) {log_entry.bytes += additional_bytes;}
    void setBuffer(std::vector<char>&& new_buffer) {buffer = std::move(new_buffer);}
    void setRequest(http::Request&& new_request) {request = std::move(new_request);}

    std::vector<char>* getBuffer() {return &buffer;}

    http::Response& GetResponse() {
        return response;
    }

    logger::SessionEntry* getLogEntry() {
        return &log_entry;
    }

    io::Socket* GetSocket() {
        return socket_.get();
    }

    http::Request& GetRequest() {
        return request;
    }


private:
    io::SocketPtr socket_ { nullptr };
    std::vector<char> buffer;
    std::function<asio::awaitable<void>()> finish;
    http::Request request;
    http::Response response;
    logger::SessionEntry log_entry;
};

