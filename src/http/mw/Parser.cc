#include "http/mw/Parser.h"
#include "http/Exception.h"
#include "http.h"

namespace mw {

asio::awaitable<void> Parser::Process(http::Transaction& txn, Next next) {
    txn.GetLogEntry()->Latency_end_time = std::chrono::system_clock::now();

    auto& buffer = txn.GetBuffer();
    auto [ec, bytes] = co_await txn.GetSocket().Read(buffer);
    if(ec) {
        throw (ec.value() == asio::error::connection_reset || ec.value() == asio::error::broken_pipe || ec.value() == asio::error::eof) ?
                http::Exception(http::Client_Closed_Request) : http::Exception(http::Internal_Server_Error);
    }
    buffer.resize(bytes);

    http::Request request;
    request.SetPath(http::extract_endpoint(buffer))
        .SetMethod(http::extract_method(buffer))
        .SetHeaders(http::extract_headers(buffer))
        .SetQueryParams(http::extract_query_params(buffer))
        .SetBody(http::extract_body(buffer));

    TRACE("MW Parser", "Hit for endpoint: %s", request.GetPath().c_str());

    txn.Route = &router_.GetRoute(request.GetPath());
    txn.Endpoint = &txn.Route->GetEndpoint(request.GetMethod());

    txn.SetRequest(std::move(request));

    co_await next();
}

}