#include "Router.h"
#include "config.h"
#include "http.h"
#include "Transaction.h"
#include "MethodHandler.h"

using namespace http;
using namespace cfg;

Router Router::INSTANCE;

static http::Endpoint DEFAULT_ENDPOINT;



Router::Router() {
    DEFAULT_ENDPOINT.addMethod({
    .m = http::method::Get,
    .access_role = VIEWER_ROLE_HASH,
    .auth_role = "",
    .is_protected = false,
    .is_authenticator = false,
    .script = "",
    .handler = [](Transaction* txn) -> asio::awaitable<void> {
        GetHandler handler(txn);
        co_await handler.handle();
        co_return;
        }
    });

    DEFAULT_ENDPOINT.addMethod({
        .m = http::method::Head,
        .access_role = VIEWER_ROLE_HASH,
        .auth_role = "",
        .is_protected = false,
        .is_authenticator = false,
        .script = "",
        .handler = [](Transaction* txn) -> asio::awaitable<void> {
            HeadHandler handler(txn);
            co_await handler.handle();
            co_return;
        }
    });

    DEFAULT_ENDPOINT.addMethod({
        .m = http::method::Post,
        .access_role = VIEWER_ROLE_HASH,
        .auth_role = "",
        .is_protected = false,
        .is_authenticator = false,
        .script = "",
        .handler = [](Transaction* txn) -> asio::awaitable<void> {
            PostHandler handler(txn);
            co_await handler.handle();
            co_return;
        }
    });
}

const Router* Router::getInstance() {
    return &Router::INSTANCE;
}

const http::Endpoint* Router::getEndpoint(const std::string& endpoint) const {
    // This function opens up caching possibilities here too, frequent requests 
    // for routes could be cached by swaping in a new lambda handler that caches
    // the response of the existing one
    auto it = endpoints.find(endpoint);
    if(it == endpoints.end()) {
        return &DEFAULT_ENDPOINT;
    }
    return &it->second;
}

static http::Handler assign_handler(method m) {
    switch (m) {
        case http::method::Get: return [](Transaction* txn) -> asio::awaitable<void> {
            GetHandler handler(txn);
            co_await handler.handle();
            co_return;
        };
        case http::method::Post: return [](Transaction* txn) -> asio::awaitable<void> {
            PostHandler handler(txn);
            co_await handler.handle();
            co_return;
        };
        case http::method::Head: return [](Transaction* txn) -> asio::awaitable<void> {
            HeadHandler handler(txn);
            co_await handler.handle();
            co_return;
        };
        default:
            return http::Handler{};
    }
}

void http::Router::updateEndpoint(const std::string& endpoint_url, EndpointMethod&& method) {
    auto it = endpoints.find(endpoint_url);
    if(it != endpoints.end()) {
        http::Endpoint& endpoint = it->second;
        method.handler = assign_handler(method.m);
        endpoint.addMethod(std::move(method));
        return;
    }
    http::Endpoint endpoint;
    method.handler = assign_handler(method.m);
    endpoint.addMethod(std::move(method));
    endpoints[endpoint_url] = endpoint;
}

http::Endpoint::Endpoint() {}

void http::Endpoint::addMethod(EndpointMethod&& method) {
    methods[method.m] = std::move(method);
}

bool http::Endpoint::isMethodProtected(method m) const {
    auto it = methods.find(m);
    if(it == methods.end()) {
        return false; // shouldnt reach, should maybe throw an http::exception (Method_Not_Allowed)
    }
    return it->second.is_protected; // corrected to return is_protected
}

bool http::Endpoint::isMethodAuthenticator(method m) const {
    auto it = methods.find(m);
    if(it == methods.end()) {
        return false; // shouldnt reach, should maybe throw an http::exception (Method_Not_Allowed)
    }
    return it->second.is_authenticator;
}

std::string http::Endpoint::getScript(method m) const {
    auto it = methods.find(m);
    if(it == methods.end()) {
        return ""; // shouldnt reach, should maybe throw an http::exception (Method_Not_Allowed)
    }
    return it->second.script;
}

std::string http::Endpoint::getAccessRole(http::method m) const {
    auto it = methods.find(m);
    if(it == methods.end()) {
        return ""; // could swap in the default role here, maybe warn too
    }
    return it->second.access_role;
}

http::Handler http::Endpoint::getHandler(http::method m) const {
    auto it = methods.find(m);
    if(it == methods.end()) {
        return Handler{}; // this might be a 404, or a non registered resource, like an index.html
    }
    return it->second.handler;
}
