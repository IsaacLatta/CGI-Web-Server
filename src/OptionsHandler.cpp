#include "MethodHandler.h"

static std::string get_methods_str(const std::vector<http::method>& methods) {
    std::string allow_header = "";
    for(auto method : methods) {
        allow_header += http::method_enum_to_str(method) + ", ";
    }
    if(!allow_header.empty()) {
        allow_header.erase(allow_header.size() - 2, 2);
    }
    return allow_header;
}

asio::awaitable<void> OptionsHandler::handle() {
    if(!request || !(request->route)) {
        throw http::HTTPException(http::code::Internal_Server_Error, "OPTIONS request for unexpected missing route or endpoint");
    }

    response->addHeader("Allow", get_methods_str(request->route->getMethods()));
    response->addHeader("Content-Length", "0");
    response->addHeader("Connection", "close");
    std::string response_str = response->build();
    co_await txn->getSocket()->co_write(response_str.data(), response_str.length());
    co_return;
}
