#include "io/Acceptor.h"
#include "io/Socket.h"

namespace io {

    DefaultAcceptor::DefaultAcceptor(asio::io_context& io_context, const asio::ip::tcp::endpoint &endpoint)
            : io_context_(io_context), acceptor_(io_context, endpoint) {}

    DefaultAcceptor::~DefaultAcceptor() { DefaultAcceptor::Close(); }

    void DefaultAcceptor::Close() {
        if (acceptor_.is_open()) {
            asio::error_code ignored;
            (void)acceptor_.cancel(ignored);
            (void)acceptor_.close(ignored);
        }
    }

    asio::awaitable<std::pair<SocketPtr, asio::error_code>> DefaultAcceptor::Accept() {
        asio::ip::tcp::socket socket(io_context_);

        asio::error_code ec;
        co_await acceptor_.async_accept(socket, asio::redirect_error(asio::use_awaitable, ec));
        if (ec) {
            co_return std::pair<SocketPtr, asio::error_code> { nullptr, ec };
        }

        co_return std::pair<SocketPtr, asio::error_code> {
            std::make_unique<HTTPSocket>(std::move(socket)),
            asio::error_code{}
        };
    }

}