#pragma once

#include <memory>

namespace io {

    class Socket;

    using SocketPtr = std::unique_ptr<Socket>;

    class Acceptor;

    using AcceptorPtr = std::unique_ptr<Acceptor>;

    /* Estimated BDP for typical network conditions, e.g.) RTT=20 ms, BW=100-200 Mbps*/
    constexpr size_t BUFFER_SIZE { 262144u };

    // TODO: Move me somewhere else
    struct TransferState {
        long total_bytes{0};
        long bytes_sent{0};
        long current_offset{0};
        int retry_count{1};
        static constexpr int MAX_RETRIES { 3 };
        static constexpr auto RETRY_DELAY { std::chrono::milliseconds(100) };
    };
}