#pragma once

#include <memory>
#include <functional>
#include <asio.hpp>

#include "io/forward.h"

namespace http {

    class Session;

    using SessionPtr = std::shared_ptr<Session>;

    using SessionFactory = std::function<SessionPtr(::io::SocketPtr&&)>;

    class Server;

    using SocketFactory = std::function<::io::SocketPtr(const asio::io_context&)>;

    class Server;

    class Response;

    class HTTPException;

    using Headers = std::unordered_map<std::string, std::string>;

}

namespace mw {

    class Middleware;

    class Pipeline;

    using Next = std::function<asio::awaitable<void>()>;
}
