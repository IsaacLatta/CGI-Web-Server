#pragma once

#include <asio.hpp>
#include <asio/awaitable.hpp>

#include "io/forward.h"

namespace io {

    class Acceptor {
    public:
        virtual ~Acceptor() = default;

        virtual void Close() = 0;
        virtual asio::awaitable<std::pair<SocketPtr, asio::error_code>> Accept() = 0;

    };

    class DefaultAcceptor: public Acceptor {
    public:
        DefaultAcceptor(asio::io_context&, const asio::ip::tcp::endpoint&);
        ~DefaultAcceptor() override;

        void Close() override;
        asio::awaitable<std::pair<SocketPtr, asio::error_code>> Accept() override;

    private:
        asio::io_context& io_context_;
        asio::ip::tcp::acceptor acceptor_;
    };
}
