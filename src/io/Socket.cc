#include "Socket.h"

namespace io {

    PlainSocket::PlainSocket(asio::ip::tcp::socket&& sock) : socket_(std::move(sock)) {
        TryCacheIp();
    }

    asio::ip::tcp::socket &PlainSocket::GetRawSocket() {
        return socket_;
    }

    std::string PlainSocket::IpStr() const {
        if (!client_address_.empty()) {
            return client_address_;
        }

        if(socket_.is_open()) {
            return socket_.remote_endpoint().address().to_string();
        }

        return "";
    }

    void PlainSocket::TryCacheIp() {
        client_address_ = IpStr();
    }

    asio::awaitable<Socket::Result> PlainSocket::Read(std::span<char> buffer) {
        auto [ec, bytes_read] = co_await socket_.async_read_some(asio::buffer(buffer), asio::as_tuple(asio::use_awaitable));
        co_return Result { ec, bytes_read };
    }

    asio::awaitable<Socket::Result> PlainSocket::Write(std::span<const char> buffer) {
        auto [ec, bytes_written] = co_await asio::async_write(socket_, asio::buffer(buffer), asio::as_tuple(asio::use_awaitable));
        co_return Result{ ec, bytes_written };
    }

    void PlainSocket::Close() {
        if(socket_.is_open()) {
            socket_.close();
        }
    }

    PlainSocket::~PlainSocket() {
        Close();
    }
}