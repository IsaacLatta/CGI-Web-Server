#include "RequestHandler.h" 
#include "Session.h"

asio::awaitable<void> PostHandler::handle() {
    auto this_session = session.lock();
    if(!this_session) {
        ERROR("POST Handler", 0, "NULL", "failed to lock session observer");
    }

    http::code code;
    std::string endpoint, body, content_type;
    if( (code = http::extract_endpoint(buffer, endpoint)) != http::code::OK || 
        (code = http::extract_body(buffer, body)) != http::code::OK ||
        (code = http::find_content_type(buffer, content_type)) != http::code::OK)
    {
        this_session->onError(http::error(code, "Failed to parse POST request"));
        co_return;
    }

    config::Route route = routes[endpoint];
    if(route.method != "POST" && route.method != "post") {
        this_session->onError(http::error(http::code::Method_Not_Allowed, std::format("Attempt to access {} with POST, allowed={}", endpoint, route.method)));
        co_return;
    }
    
    if(content_type == "application/x-www-form-urlencoded") {
        // Parse form encoded args
    }
    else if (content_type == "application/json") {
        // Parse json encoded args
    }
    else {
        this_session->onError(http::error(http::code::Not_Implemented, std::format("Content-Type: {} not supported", content_type)));
    }
    LOG("INFO", "POST Handler", "REQUEST\n%s\n\nPARSED RESULTS\nEndpoint: %s\nContent-Type: %s\nBody: %s", buffer.data(), endpoint.c_str(), content_type.c_str(), body.c_str());
}

