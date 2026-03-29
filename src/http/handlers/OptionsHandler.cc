#include "MethodHandler.h"

#include "http/Exception.h"

static std::string get_methods_str(const std::vector<http::Method>& methods) {
    std::string allow_header;
    for(const auto method : methods) {
        allow_header += std::string(http::method_enum_to_str(method)) + ", ";
    }
    if(!allow_header.empty()) {
        allow_header.erase(allow_header.size() - 2, 2);
    }
    return allow_header;
}

asio::awaitable<void> OptionsHandler::Handle() {
    auto& request = txn_.GetRequest();

    if(!request.endpoint) {
        throw http::HTTPException(http::Internal_Server_Error, "OPTIONS request for unexpected missing route or endpoint");
    }

    std::string response_str = txn_.GetResponse()
        .AddHeader("Allow", get_methods_str(request.endpoint->getMethods()))
        .AddHeader("Content-Length", "0")
        .AddHeader("Connection", "close")
        .Build();

    co_await txn_.GetSocket()->Write(response_str);
    co_return;
}
