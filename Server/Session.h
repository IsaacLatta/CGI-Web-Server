
#ifndef SESSION_H
#define SESSION_H

#include <asio.hpp>
#include <asio/ssl.hpp>
#include <vector>
#include <memory>
#include <iostream>
#include <chrono>

#include "Socket.h"
#include "Transaction.h"
#include "logger.h"
#include "http.h"

#define BUFFER_SIZE 65536

class Session : public std::enable_shared_from_this<Session>
{
    public:
    long bytes_transferred;
    std::chrono::time_point<std::chrono::system_clock> start_time;
    std::chrono::time_point<std::chrono::system_clock> end_time;

    Session(std::unique_ptr<Socket>&& sock) : _sock(std::move(sock)), _transaction(std::make_unique<Transaction>()), bytes_transferred(0) {};
    void start();
    Socket* get_socket();
    Transaction* get_transaction();
    
    private:
    std::unique_ptr<Transaction> _transaction;
    std::unique_ptr<Socket> _sock;
    std::optional<asio::posix::stream_descriptor> file_desc;
    std::vector<char> _buffer;
    void handle_request(const asio::error_code& error);
    void send_resource(std::size_t bytes_read);
    void start_send();
};

#endif
