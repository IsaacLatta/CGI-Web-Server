#include "http/http.h"
#include "io/Socket.h"
#include "io/io.h"
#include "http/Response.h"
#include "../io/Streamer.h"
#include "logger/macros.h"

namespace http {

    asio::awaitable<void> send_response(Response& response, io::Socket& sock) {
        const auto [ec, bytes] = co_await send_response_to(response, sock);
        if (ec) {
            ERROR("http::send_response", "Failed to send response (code=%d) to client %s, error=%s",
                static_cast<int>(response.Status), sock.GetIpStr().c_str(), ec.message().c_str());
        }
        co_return;
    }

    asio::awaitable<io::Result> send_response_to(Response& response, io::Socket& sock) {
        std::string response_str = response.Build();
        io::StringStreamer streamer(response_str);
        co_return co_await streamer.Stream(sock);
    }

}