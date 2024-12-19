#ifndef TRANSACTION_H
#define TRANSACTION_H

#include "http.h"
#include "logger.h"
#include <vector>

struct Transaction {
    std::weak_ptr<Session> session;
    Socket* sock;
    std::vector<char> buffer;
    http::Response response;
    logger::Entry log_entry;
    long bytes = 0;

    Transaction(std::weak_ptr<Session> session, Socket* sock, std::vector<char>&& buffer)
        : session(std::move(session)), sock(sock), buffer(std::move(buffer)) {}

    void addBytes(long additional_bytes) { bytes += additional_bytes; }
    void setBuffer(std::vector<char>&& new_buffer) { buffer = std::move(new_buffer); }

    const std::vector<char>* getBuffer() const { return &buffer; }
    http::Response* getResponse() { return &response; }
    logger::Entry* getLogEntry() { return &log_entry; }
};

#endif
