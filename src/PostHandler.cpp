#include "RequestHandler.h" 
#include "Session.h"

asio::awaitable<void> PostHandler::handle() {
    auto this_session = session.lock();
    if(!this_session) {
        ERROR("POST Handler", 0, "NULL", "failed to lock session observer");
    }
    LOG("INFO", "POST Handler", "REQUEST %s", buffer.data());

    http::code code;
    std::string endpoint, body;
    if((code = http::extract_endpoint(buffer, endpoint)) != http::code::OK || (code = http::extract_body(buffer, body)) != http::code::OK) {
        this_session->onError(http::error(code, "Failed to extract the endpoint for POST request"));
        co_return;
    }

    LOG("INFO", "POST Handler", "PARSED RESULTS\nEndpoint: %s\nbody: %s", endpoint.c_str(), body.c_str());
    config::Route route = routes[endpoint];
    if(route.method != "POST" || route.method != "post") {
        this_session->onError(http::error(http::code::Method_Not_Allowed, std::format("Attempt to access {} with POST, allowed={}", endpoint, route.method)));
        co_return;
    }
}

