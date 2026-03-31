#pragma once

#include <vector>

#include "parsing/parse.h"

#include "logger/Entry.h"
#include "http/Response.h"
#include "http/Request.h"

namespace http {
class Transaction {
public:
    const Route* ResolvedRoute { nullptr };
    const Endpoint* ResolvedEndpoint { nullptr };

    Transaction(io::SocketPtr&& sock, size_t buffer_size = io::BUFFER_SIZE):
        socket_(std::move(sock)) {

        buffer_.reserve(buffer_size);
        buffer_.resize(buffer_size);
    }

    Response& GetResponse() {
        return response_;
    }

    io::Socket& GetSocket() {
        return *socket_;
    }

    Request& GetRequest() {
        return request_;
    }

    void SetRequest(Request&& request) {
        request_ = std::move(request);
    }

    logger::SessionEntry& GetLogEntry() & {
        return log_entry_;
    }

    const logger::SessionEntry& GetLogEntry() const & {
        return log_entry_;
    }

    void addBytes(long additional_bytes) {log_entry_.BytesServed += additional_bytes;}
    void setBuffer(std::vector<char>&& new_buffer) {buffer_ = std::move(new_buffer);}

    void SetResponse(Response request) {
        response_ = std::move(request);
    }

    std::vector<char>& GetBuffer() {
        return buffer_;
    }

    std::span<const char> ViewBuffer() const {
        return buffer_;
    }

private:
    io::SocketPtr socket_ { nullptr };
    Request request_;
    Response response_;
    logger::SessionEntry log_entry_{};
    std::vector<char> buffer_;
};

}



