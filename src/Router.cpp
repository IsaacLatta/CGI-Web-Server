#include "Router.h"
#include "config.h"
#include "http.h"
#include "Transaction.h"
#include "MethodHandler.h"

using namespace http;
using namespace cfg;

static http::Endpoint DEFAULT_ENDPOINT;
static http::ErrorPage DEFAULT_ERROR_PAGE;
Router Router::INSTANCE;

Router::Router() {
    // DEFAULT_ENDPOINT.setEndpointURL("/");

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

    DEFAULT_ERROR_PAGE.handler = [](Transaction* txn) -> asio::awaitable<void> {
        std::string response = txn->response.build();
        txn->sock->write(response.data(), response.length());
        co_return;
    };
}

http::arg_type http::arg_str_to_enum(const std::string& args_str) noexcept {
    std::string_view args = args_str;
    std::string args_upper = http::trim_to_upper(args);
    if(args_upper == "JSON") {
        return http::arg_type::Body_JSON;
    } else if (args_upper == "QUERY") {
        return http::arg_type::Query_String;
    } else if (args_upper == "URL") {
        return http::arg_type::Body_URL;
    } else if(args_upper == "*" || args_upper == "ANY") {
        return http::arg_type::Any;
    } else if(args_upper == "BODY") {
        return http::arg_type::Body_Any;
    }
    return http::arg_type::None;
}

const Router* Router::getInstance() {
    return &Router::INSTANCE;
}

static bool file_exists(const std::string& path) {
    struct stat buffer;
    return stat(path.c_str(), &buffer) == 0;
}

const http::Endpoint* Router::getEndpoint(const std::string& endpoint) const {
    auto it = endpoints.find(endpoint);
    if(it == endpoints.end()) {
        if (file_exists("public/" + endpoint)) {
            return &DEFAULT_ENDPOINT;
        }
        throw http::HTTPException(http::code::Not_Found, std::format("request for {}: does not exist", endpoint));
    }
    return &it->second;
}

const http::ErrorPage* Router::getErrorPage(http::code status) const {
    auto it = error_pages.find(status);
    if(it == error_pages.end()) {
        return &DEFAULT_ERROR_PAGE;
    }
    return &it->second;
}

void Router::addErrorPage(ErrorPage&& error_page, std::string&& file) {
    error_page.handler = [file_path = std::move(file), status = error_page.status](Transaction* txn) -> asio::awaitable<void> {
        try {
            FileStreamer f_stream(file_path);
            txn->getResponse()->addHeader("Content-Length", std::to_string(f_stream.getFileSize()));
            std::string response = txn->getResponse()->build();
            StringStreamer s_stream(&response);
            co_await s_stream.stream(txn->getSocket());
            co_await f_stream.stream(txn->getSocket());
            co_return;
        } catch (const std::exception& error) {
            WARN("Error Page Handler", "failed to serve error page %s for code=%d: %s", 
            file_path.c_str(), static_cast<int>(status), error.what());
        } 
        
        co_await DEFAULT_ERROR_PAGE.handler(txn); // if sending the error page fails, simply send the original error
        co_return;
    };
    error_pages[error_page.status] = std::move(error_page);
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
        case http::method::Options: return [](Transaction* txn) -> asio::awaitable<void> {
            OptionsHandler handler(txn);
            co_await handler.handle();
            co_return;
        };
        default:
            throw http::HTTPException(http::code::Not_Implemented, "request method not supported");
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

void http::Endpoint::setEndpointURL(const std::string& url) {
    endpoint = url;
}

void http::Endpoint::addMethod(EndpointMethod&& method) {
    methods[method.m] = std::move(method);
}

bool http::Endpoint::isMethodProtected(method m) const {
    auto it = methods.find(m);
    if(it == methods.end()) {
        throw http::HTTPException(http::code::Method_Not_Allowed, 
        std::format("failed to retrieve protection status: {} {} does not exist", method_enum_to_str(m), endpoint)); 
    }
    return it->second.is_protected; // corrected to return is_protected
}

arg_type http::Endpoint::getArgType(http::method m) const {
    auto it = methods.find(m);
    if(it == methods.end()) {
        throw http::HTTPException(http::code::Method_Not_Allowed, 
        std::format("failed to retrieve argument type: {} {} does not exist", method_enum_to_str(m), endpoint));  
    }
    return it->second.args;
}

std::string http::Endpoint::getAuthRole(method m) const {
    auto it = methods.find(m);
    if(it == methods.end()) {
        throw http::HTTPException(http::code::Method_Not_Allowed, 
        std::format("failed to retrieve auth role: {} {} does not exist", method_enum_to_str(m), endpoint)); 
    }
    return it->second.auth_role;
}

bool http::Endpoint::isMethodAuthenticator(method m) const {
    auto it = methods.find(m);
    if(it == methods.end()) {
        throw http::HTTPException(http::code::Method_Not_Allowed, 
        std::format("failed to retrieve authenticator status: {} {} does not exist", method_enum_to_str(m), endpoint)); 
    }
    return it->second.is_authenticator;
}

std::string http::Endpoint::getScript(method m) const {
    auto it = methods.find(m);
    if(it == methods.end()) {
        throw http::HTTPException(http::code::Method_Not_Allowed, 
        std::format("failed to retrieve script: {} {} does not exist", method_enum_to_str(m), endpoint)); 
    }
    return it->second.script;
}

std::string http::Endpoint::getAccessRole(http::method m) const {
    auto it = methods.find(m);
    if(it == methods.end()) {
        throw http::HTTPException(http::code::Method_Not_Allowed, 
        std::format("failed to retrieve access role: {} {} does not exist", method_enum_to_str(m), endpoint)); 
    }
    return it->second.access_role;
}

http::Handler http::Endpoint::getHandler(http::method m) const {
    auto it = methods.find(m);
    if(it == methods.end()) {
        if(m != http::method::Get && m != http::method::Head) {
            throw http::HTTPException(http::code::Method_Not_Allowed, 
            std::format("failed to retrieve handler {} {} does not exist", http::method_enum_to_str(m), endpoint));
        } 
        return assign_handler(m); 
    }
    return it->second.handler;
}

std::vector<method> http::Endpoint::getMethods() const {
    std::vector<method> results;
    for(auto m: methods) {
        results.push_back(m.first);
    }
    return results;
}
