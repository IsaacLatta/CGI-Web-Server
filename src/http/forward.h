#pragma once

#include <memory>
#include <functional>
#include <asio.hpp>

#include "io/forward.h"

namespace http {

    enum class Code {
        Not_A_Status = -1,
        OK = 200,
        Created = 201,
        Accepted = 202,
        No_Content = 204,
        Moved_Permanently = 301,
        Found = 302,
        See_Other = 303,
        Not_Modified = 304,
        Bad_Request = 400,
        Unauthorized = 401,
        Forbidden = 403,
        Not_Found = 404,
        Method_Not_Allowed = 405,
        Unsupported_Media_Type = 415,
        Too_Many_Requests = 429,
        Client_Closed_Request = 499,
        Internal_Server_Error = 500,
        Not_Implemented = 501,
        Bad_Gateway = 502,
        Service_Unavailable = 503,
        Gateway_Timeout = 504,
        Insufficient_Storage = 507
    }; using enum Code;

    enum class Method {
        Unassigned = -1,
        Get,
        Head,
        Post,
        Options,
        Not_Allowed
    }; using enum Method;

    enum class ArgumentType {
        None = 0,
        Any,
        Body_Any,
        Body_JSON,
        Body_URL,
        Query_String
    };

    class Session;

    using SessionPtr = std::shared_ptr<Session>;

    using SessionFactory = std::function<SessionPtr(::io::SocketPtr&&)>;

    class Server;

    using SocketFactory = std::function<::io::SocketPtr(const asio::io_context&)>;

    class Server;

    class Response;

    class Request;

    class Exception;

    using Headers = std::unordered_map<std::string, std::string>;

    using QueryParams = std::unordered_map<std::string_view, std::string_view>;

    class MethodHandler;

    class Transaction;

    struct ErrorPage;

    struct Endpoint;

    class Route;

    class Router;

    using EndpointFactory = std::function<Route(std::string path)>;

    using ErrorPageFactory = std::function<ErrorPage(Code)>;

    using Handler = std::function<asio::awaitable<void>(Transaction*)>;
    using Limiter = std::function<asio::awaitable<void>(Transaction*)>;

    using HandlerFactory = std::function<Handler(Method)>;
}

namespace mw {

    class Middleware;

    class Pipeline;

    using Next = std::function<asio::awaitable<void>()>;
}
