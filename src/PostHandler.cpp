#include "RequestHandler.h" 
#include "Session.h"

asio::awaitable<void> PostHandler::handle() {
    auto this_session = session.lock();
    if(!this_session) {
        ERROR("POST Handler", 0, "NULL", "failed to lock session observer");
    }

    http::code code;
    std::string endpoint;
    if( (code = http::extract_endpoint(buffer, endpoint)) != http::code::OK ) {
        this_session->onError(http::error(code, "Failed to parse POST request"));
        co_return;
    }

    config::Route route = routes[endpoint];
    if(route.method != "POST" && route.method != "post") {
        this_session->onError(http::error(http::code::Method_Not_Allowed, std::format("Attempt to access {} with POST, allowed={}", endpoint, route.method)));
        co_return;
    }
    
    http::json args;
    if((code = http::build_json(buffer, args)) != http::code::OK) {
        this_session->onError(http::error(code, "Failed to build json array"));
        co_return;
    }

    LOG("INFO", "POST Handler", "REQUEST PARSING SUCCEDED\n%s\n\nPARSED RESULTS\nEndpoint: %s\nJSON Args: %s", buffer.data(), endpoint.c_str(), args.dump().c_str());
}

