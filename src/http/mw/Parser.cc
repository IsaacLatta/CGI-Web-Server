#include "http/mw/Parser.h"
#include "http/Exception.h"
#include "http.h"

namespace mw {

asio::awaitable<void> mw::Parser::Process(Transaction& txn, Next next) {
    auto buffer =  txn.getBuffer();
    auto [ec, bytes] = co_await txn.getSocket()->Read(*buffer);
    txn.getLogEntry()->Latency_end_time = std::chrono::system_clock::now();
    buffer->resize(bytes);

    if(ec) {
        throw (ec.value() == asio::error::connection_reset || ec.value() == asio::error::broken_pipe || ec.value() == asio::error::eof) ?
                http::HTTPException(http::code::Client_Closed_Request, std::format("Failed to read request from client: {}", txn.sock->IpPortStr())) :
                http::HTTPException(http::code::Internal_Server_Error, std::format("Failed to read request from client: {}", txn.sock->IpPortStr()));
    }

    auto router = http::Router::getInstance();
    http::Request request;
    request.endpoint_url = http::extract_endpoint(*buffer);
    request.endpoint = router->getEndpoint(request.endpoint_url);
    request.method = http::extract_method(*buffer);
    request.args = http::extract_args(*buffer, request.endpoint->getArgType(request.method));
    request.headers = http::extract_headers(*buffer);
    request.body = http::extract_body(*buffer);
    request.query = http::extract_query_string(*buffer);
    request.route = router->getEndpointMethod(request.endpoint_url, request.method);

    TRACE("MW Parser", "Hit for endpoint: %s", request.endpoint_url.c_str());

    txn.setRequest(std::move(request));
    co_await next();
}

}