#include "Router.h"
#include "config.h"
#include "http.h"

using namespace http;
using namespace cfg;

Router Router::INSTANCE;

Router::Router() {}

const Router* Router::getInstance() {
    return &Router::INSTANCE;
}

const http::Endpoint* Router::getEndpoint(const std::string& endpoint) const {
    // This function opens up caching possibilities here too, frequent requests 
    // for routes could be cached by swaping in a new lambda handler that caches
    // the response of the existing one
    auto it = endpoints.find(endpoint);
    if(it == endpoints.end()) {
        // Possibly create a default route here, but could cause const conflict issues.
        return nullptr;
    }
    return &it->second;
}

void http::Router::updateEndpoint(const std::string& endpoint_url, EndpointMethod&& method) {
    auto it = endpoints.find(endpoint_url);
    if(it != endpoints.end()) {
        http::Endpoint& endpoint = it->second;
        endpoint.addMethod(std::move(method));
        return;
    }
    http::Endpoint endpoint;
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
        return false; // should reach, should maybe throw an http::exception (Method_Not_Allowed)
    }
    return it->second.is_protected; // corrected to return is_protected
}

bool http::Endpoint::isMethodAuthenticator(method m) const {
    auto it = methods.find(m);
    if(it == methods.end()) {
        return false; // should reach, should maybe throw an http::exception (Method_Not_Allowed)
    }
    return it->second.is_authenticator;
}

std::string http::Endpoint::getScript(method m) const {
    auto it = methods.find(m);
    if(it == methods.end()) {
        return ""; // should reach, should maybe throw an http::exception (Method_Not_Allowed)
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
