#ifndef SOCKET_H
#define SOCKET_H

#include <asio.hpp>
#include <asio/awaitable.hpp>
#include <asio/use_awaitable.hpp>
#include <asio/ssl.hpp>
#include <vector>
#include "logger.h"

class Socket
{
    public:
    virtual void do_handshake(const std::function<void(const asio::error_code&)>& callback) = 0;
    virtual void do_read(char* buffer, std::size_t buffer_size, const std::function<void(const asio::error_code&, std::size_t)>& callback) = 0;
    virtual void do_write(char* buffer, std::size_t buffer_size, const std::function<void(const asio::error_code&, std::size_t)>& callback) = 0;
    
    virtual asio::awaitable<std::size_t> co_read(char* buffer, std::size_t size) = 0;
    virtual asio::awaitable<std::size_t> co_write(const char* buffer, std::size_t size) = 0;


    virtual std::string get_IP() = 0;
    virtual asio::ip::tcp::socket& get_raw_socket() = 0;
    virtual void close() = 0;
    virtual ~Socket() = default;
};

class HTTPSocket: public Socket
{
    public:
    HTTPSocket(asio::io_context& io_context) : _socket(io_context) {};
    ~HTTPSocket();

    asio::awaitable<std::size_t> co_read(char* buffer, std::size_t size) override {
        co_return co_await _socket.async_read_some(asio::buffer(buffer, size), asio::use_awaitable);
    }

    asio::awaitable<std::size_t> co_write(const char* buffer, std::size_t size) override {
        co_return co_await asio::async_write(_socket, asio::buffer(buffer, size), asio::use_awaitable);
    }

    void do_handshake(const std::function<void(const asio::error_code&)>& callback) override;
    void do_read(char* buffer, std::size_t buffer_size, const std::function<void(const asio::error_code&, std::size_t)>& callback) override;
    void do_write(char* buffer, std::size_t buffer_size, const std::function<void(const asio::error_code&, std::size_t)>& callback) override;
    void close() override;
    std::string get_IP() override;
    asio::ip::tcp::socket& get_raw_socket() override;
    private:
    asio::ip::tcp::socket _socket;
};

class HTTPSSocket : public Socket
{
    public:
    HTTPSSocket(asio::io_context& io_context, asio::ssl::context& ssl_context): _socket(io_context, ssl_context) {};
    ~HTTPSSocket();

    asio::awaitable<std::size_t> co_read(char* buffer, std::size_t buffer_size) {
        // Perform asynchronous read, awaiting the result
        std::size_t bytes_read = co_await _socket.async_read_some(asio::buffer(buffer, buffer_size), asio::use_awaitable);
        co_return bytes_read;
    }

    // Coroutine-friendly async write for SSL
    asio::awaitable<std::size_t> co_write(const char* buffer, std::size_t buffer_size) {
        // Perform asynchronous write, awaiting the result
        std::size_t bytes_written = co_await asio::async_write(_socket, asio::buffer(buffer, buffer_size), asio::use_awaitable);
        co_return bytes_written;
    }
    
    void do_handshake(const std::function<void(const asio::error_code&)>& callback) override;
    void do_read(char* buffer, std::size_t buffer_size, const std::function<void(const asio::error_code&, std::size_t)>& callback) override;
    void do_write(char* buffer, std::size_t buffer_size, const std::function<void(const asio::error_code&, std::size_t)>& callback) override;
    void close();
    std::string get_IP() override;
    asio::ip::tcp::socket& get_raw_socket() override;
    private:
    asio::ssl::stream<asio::ip::tcp::socket> _socket;
};

#endif

