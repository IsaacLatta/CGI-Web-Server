#include "io/Acceptor.h"
#include "io/Socket.h"

namespace io {

    PlainAcceptor::PlainAcceptor(asio::io_context& io_context, const asio::ip::tcp::endpoint &endpoint)
            : acceptor_(io_context, endpoint) {}

    PlainAcceptor::~PlainAcceptor() { PlainAcceptor::Close(); }

    void PlainAcceptor::Close() {
        if (acceptor_.is_open()) {
            asio::error_code ignored;
            (void)acceptor_.cancel(ignored);
            (void)acceptor_.close(ignored);
        }
    }

    asio::awaitable<AcceptResult> PlainAcceptor::Accept() {
        asio::ip::tcp::socket socket(acceptor_.get_executor());

        asio::error_code ec;
        co_await acceptor_.async_accept(socket, asio::redirect_error(asio::use_awaitable, ec));
        if (ec) {
            co_return AcceptResult { ec, nullptr };
        }

        co_return AcceptResult {
            asio::error_code{},
            std::make_unique<PlainSocket>(std::move(socket))
        };
    }

}