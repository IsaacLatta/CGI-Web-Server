#ifndef TRANSACTION_H
#define TRANSACTION_H

#include "http.h"
#include "logger.h"
#include <vector>

class Session;
class Socket;


struct TransferState {
    long total_bytes{0};         
    long bytes_sent{0};          
    long current_offset{0};      
    int retry_count{1};
    static constexpr int MAX_RETRIES = 3;
    static constexpr auto RETRY_DELAY = std::chrono::milliseconds(100);
};

struct Transaction {
    Socket* sock;
    std::vector<char> buffer;
    std::function<asio::awaitable<void>()> finish;
    http::Request request;
    http::Response response;
    logger::SessionEntry log_entry;

    Transaction(Socket* sock): sock(sock), buffer(BUFSIZ), finish(nullptr) {}
    void addBytes(long additional_bytes) {log_entry.bytes += additional_bytes;}
    void setBuffer(std::vector<char>&& new_buffer) {buffer = std::move(new_buffer);}
    void setRequest(http::Request&& new_request) {request = std::move(new_request);}

    std::vector<char>* getBuffer() {return &buffer;}
    http::Response* getResponse() {return &response;}
    logger::SessionEntry* getLogEntry() {return &log_entry;}
    Socket* getSocket() {return sock;}
    http::Request* getRequest() {return &request;}
};

#endif
