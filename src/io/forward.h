#pragma once

#include <memory>

namespace io {

    class Socket;

    using SocketPtr = std::unique_ptr<Socket>;

    class Acceptor;

    using AcceptorPtr = std::unique_ptr<Acceptor>;

    /* Estimated BDP for typical network conditions, e.g.) RTT=20 ms, BW=100-200 Mbps*/
    constexpr size_t BUFFER_SIZE { 262144u };

}