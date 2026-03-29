#ifndef SOCKET_H
#define SOCKET_H

#include <asio.hpp>
#include <asio/awaitable.hpp>

#include <span>

#include "io/forward.h"

namespace io {

class Socket {
public:
    struct Result {
        asio::error_code ec;
        size_t bytes { 0u };
    };

public:
    virtual ~Socket() = default;
    virtual asio::awaitable<Result> Read(std::span<char>) = 0;
    virtual asio::awaitable<Result> Write(std::span<const char>) = 0;

    // TODO: Remove these from the interface.
public:
    virtual asio::ip::tcp::socket& GetRawSocket() = 0;
    virtual std::string IpStr() const { return ""; }
};

class PlainSocket: public Socket {
public:
    explicit PlainSocket(asio::ip::tcp::socket&&);
    ~PlainSocket() override;

    asio::awaitable<Result> Read(std::span<char>) override;
    asio::awaitable<Result> Write(std::span<const char>) override;


    std::string IpStr() const override;
    asio::ip::tcp::socket& GetRawSocket() override;

private:
    void Close();
    void TryCacheIp();

private:
    std::string client_address_;
    asio::ip::tcp::socket socket_;
};

}

#endif

