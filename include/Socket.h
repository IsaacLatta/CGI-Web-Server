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
    virtual void handshake(const std::function<void(const asio::error_code&)>& callback) = 0;
    virtual void read(char* buffer, std::size_t buffer_size, const std::function<void(const asio::error_code&, std::size_t)>& callback) = 0;
    virtual void write(char* buffer, std::size_t buffer_size, const std::function<void(const asio::error_code&, std::size_t)>& callback) = 0;
    
    virtual asio::awaitable<asio::error_code> co_handshake() = 0;
    virtual asio::awaitable<std::tuple<asio::error_code, std::size_t>> co_read(char* buffer, std::size_t size) = 0;
    virtual asio::awaitable<std::tuple<asio::error_code, std::size_t>> co_write(const char* buffer, std::size_t size) = 0;

    virtual std::string getIP() = 0;
    virtual asio::ip::tcp::socket& getRawSocket() = 0;
    virtual void close() = 0;
    virtual ~Socket() = default;
};

class HTTPSocket: public Socket
{
    public:
    HTTPSocket(asio::io_context& io_context) : _socket(io_context) {};
    ~HTTPSocket();

    asio::awaitable<asio::error_code> co_handshake() override;
    asio::awaitable<std::tuple<asio::error_code, std::size_t>> co_read(char* buffer, std::size_t size) override;
    asio::awaitable<std::tuple<asio::error_code, std::size_t>> co_write(const char* buffer, std::size_t size) override;
   
    void handshake(const std::function<void(const asio::error_code&)>& callback) override;
    void read(char* buffer, std::size_t buffer_size, const std::function<void(const asio::error_code&, std::size_t)>& callback) override;
    void write(char* buffer, std::size_t buffer_size, const std::function<void(const asio::error_code&, std::size_t)>& callback) override;
    
    void close() override;
    std::string getIP() override;
    asio::ip::tcp::socket& getRawSocket() override;
    
    private:
    asio::ip::tcp::socket _socket;
};

class HTTPSSocket : public Socket
{
    public:
    HTTPSSocket(asio::io_context& io_context, asio::ssl::context& ssl_context): _socket(io_context, ssl_context) {};
    ~HTTPSSocket();

    asio::awaitable<asio::error_code> co_handshake() override;
    asio::awaitable<std::tuple<asio::error_code, std::size_t>> co_read(char* buffer, std::size_t buffer_size) override;
    asio::awaitable<std::tuple<asio::error_code, std::size_t>> co_write(const char* buffer, std::size_t buffer_size) override;
    
    void handshake(const std::function<void(const asio::error_code&)>& callback) override;
    void read(char* buffer, std::size_t buffer_size, const std::function<void(const asio::error_code&, std::size_t)>& callback) override;
    void write(char* buffer, std::size_t buffer_size, const std::function<void(const asio::error_code&, std::size_t)>& callback) override;
    
    void close();
    std::string getIP() override;
    asio::ip::tcp::socket& getRawSocket() override;
    
    private:
    asio::ssl::stream<asio::ip::tcp::socket> _socket;
};

#endif

