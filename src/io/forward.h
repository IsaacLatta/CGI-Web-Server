#pragma once

#include <memory>

namespace io {

    class Socket;

    using SocketPtr = std::unique_ptr<Socket>;

    class Acceptor;

    using AcceptorPtr = std::unique_ptr<Acceptor>;

}