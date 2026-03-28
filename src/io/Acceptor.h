#pragma once

#include <asio.hpp>
#include <asio/awaitable.hpp>

#include "io/forward.h"

namespace io {

struct AcceptResult {
    asio::error_code ec;
    SocketPtr socket;
};

class Acceptor {
public:
    virtual ~Acceptor() = default;

    virtual void Close() = 0;

    virtual asio::awaitable<AcceptResult> Accept() = 0;

};

class PlainAcceptor: public Acceptor {
public:
    PlainAcceptor(asio::io_context&, const asio::ip::tcp::endpoint&);

    ~PlainAcceptor() override;

    void Close() override;

    asio::awaitable<AcceptResult> Accept() override;

private:
    asio::ip::tcp::acceptor acceptor_;
};

}
