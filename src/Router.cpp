#include "Router.h"
#include "config.h"
#include "http.h"
#include "Transaction.h"
#include "http/handlers/MethodHandler.h"
#include "http/Exception.h"

#include "Streamer.h"

using namespace http;
using namespace cfg;

static http::Endpoint ROOT_ENDPOINT;
static http::ErrorPage DEFAULT_ERROR_PAGE;
Router Router::INSTANCE;

Router::Router() {
    ROOT_ENDPOINT.addMethod({
    .m = http::Method::Get,
    .access_role = VIEWER_ROLE_HASH,
    .auth_role = "",
    .is_protected = false,
    .is_authenticator = false,
    .resource = "public/index.html",
    .handler = [](Transaction* txn) -> asio::awaitable<void> {
        GetHandler handler(*txn);
        co_await handler.Handle();
        co_return;
        },
    .rate_limiter = {}
    });

    ROOT_ENDPOINT.addMethod({
        .m = http::Method::Head,
        .access_role = VIEWER_ROLE_HASH,
        .auth_role = "",
        .is_protected = false,
        .is_authenticator = false,
        .resource = "public/index.html",
        .handler = [](Transaction* txn) -> asio::awaitable<void> {
            HeadHandler handler(*txn);
            co_await handler.Handle();
            co_return;
        },
    .rate_limiter = {}
    });
    endpoints["/"] = ROOT_ENDPOINT;

    DEFAULT_ERROR_PAGE.handler = [](Transaction* txn) -> asio::awaitable<void> {
        std::string response = txn->response.Build();
        auto result = co_await http::co_write_all(txn->GetSocket(), std::span<const char>(response.data(), response.length()));
        if(!http::is_success_code(result.status)) {
            DEBUG("MW Error Handler", "failed to execute default error handler: status=%d, message=%s", static_cast<int>(result.status), result.message.c_str());
        }
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

Router* Router::getInstance() {
    return &Router::INSTANCE;
}

static bool file_exists(const std::string& path) {
    struct stat buffer;
    return stat(path.c_str(), &buffer) == 0;
}

static http::Handler assign_handler(Method m) {
    switch (m) {
        case http::Method::Get: return [](Transaction* txn) -> asio::awaitable<void> {
            GetHandler handler(*txn);
            co_await handler.Handle();
            co_return;
        };
        case http::Method::Post: return [](Transaction* txn) -> asio::awaitable<void> {
            PostHandler handler(*txn);
            co_await handler.Handle();
            co_return;
        };
        case http::Method::Head: return [](Transaction* txn) -> asio::awaitable<void> {
            HeadHandler handler(*txn);
            co_await handler.Handle();
            co_return;
        };
        case http::Method::Options: return [](Transaction* txn) -> asio::awaitable<void> {
            OptionsHandler handler(*txn);
            co_await handler.Handle();
            co_return;
        };
        default:
            throw http::HTTPException(::Not_Implemented, "request Method not supported");
    }
}

static http::EndpointMethod create_default_endpoint_method(const std::string& endpoint, Method m) {
    return http::EndpointMethod{m, cfg::VIEWER_ROLE_HASH, "", false, false, endpoint, false, arg_type::None, assign_handler(m), {}};
}

const http::Endpoint* Router::getEndpoint(const std::string& endpoint) {
    auto it = endpoints.find(endpoint);
    if(it == endpoints.end()) {
        if (!file_exists("public" + endpoint)) {
            throw http::HTTPException(::Not_Found, std::format("request for {}: does not exist", endpoint));
        }
        http::Endpoint ep;
        ep.addMethod(create_default_endpoint_method("public" + endpoint, http::Method::Get));
        ep.addMethod(create_default_endpoint_method("public" + endpoint, http::Method::Head));
        endpoints[endpoint] = ep;
        return &endpoints[endpoint];
    }
    return &it->second;
}

const http::EndpointMethod* Router::getEndpointMethod(const std::string& endpoint_url, http::Method m) {
    auto endpoint = getEndpoint(endpoint_url);
    return endpoint->getMethod(m);
}

const http::ErrorPage* Router::getErrorPage(Code status) const {
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
            txn->GetResponse().AddHeader("Content-Length", std::to_string(f_stream.getFileSize()));
            std::string response = txn->GetResponse().Build();
            StringStreamer s_stream(&response);
            co_await s_stream.stream(txn->GetSocket());
            co_await f_stream.stream(txn->GetSocket());
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

void http::Router::updateEndpoint(const std::string& endpoint_url, EndpointMethod&& method) {
    auto it = endpoints.find(endpoint_url);
    if(it == endpoints.end()) {
        http::Endpoint endpoint;
        method.handler = assign_handler(method.m);
        endpoint.addMethod(std::move(method));
        endpoints[endpoint_url] = endpoint;    
        return;
    }
    http::Endpoint& endpoint = it->second;
    method.handler = assign_handler(method.m);
    endpoint.addMethod(std::move(method));
}

http::Endpoint::Endpoint() {}

void http::Endpoint::setEndpointURL(const std::string& url) {
    endpoint = url;
}

const http::EndpointMethod* http::Endpoint::getMethod(http::Method m) const {
    auto it = methods.find(m);
    if(it == methods.end()) {
        throw http::HTTPException(::Method_Not_Allowed, 
        std::format("failed to retrieve method: {} {} does not exist", method_enum_to_str(m), endpoint));
    }
    return &it->second;
}

void http::Endpoint::addMethod(EndpointMethod&& method) {
    methods[method.m] = std::move(method);
}

bool http::Endpoint::isMethodProtected(Method m) const {
    auto it = methods.find(m);
    if(it == methods.end()) {
        throw http::HTTPException(::Method_Not_Allowed, 
        std::format("failed to retrieve protection status: {} {} does not exist", method_enum_to_str(m), endpoint)); 
    }
    return it->second.is_protected; 
}

bool http::Endpoint::hasScript(Method m) const {
    auto it = methods.find(m);
    if(it == methods.end()) {
        throw http::HTTPException(::Method_Not_Allowed, 
        std::format("failed to retrieve script status: {} {} does not exist", method_enum_to_str(m), endpoint)); 
    }
    return it->second.has_script; 
}

arg_type http::Endpoint::getArgType(http::Method m) const {
    auto it = methods.find(m);
    if(it == methods.end()) {
        throw http::HTTPException(::Method_Not_Allowed, 
        std::format("failed to retrieve argument type: {} {} does not exist", method_enum_to_str(m), endpoint));  
    }
    return it->second.args;
}

std::string http::Endpoint::getAuthRole(Method m) const {
    auto it = methods.find(m);
    if(it == methods.end()) {
        throw http::HTTPException(::Method_Not_Allowed, 
        std::format("failed to retrieve auth role: {} {} does not exist", method_enum_to_str(m), endpoint)); 
    }
    return it->second.auth_role;
}

bool http::Endpoint::isMethodAuthenticator(Method m) const {
    auto it = methods.find(m);
    if(it == methods.end()) {
        throw http::HTTPException(::Method_Not_Allowed, 
        std::format("failed to retrieve authenticator status: {} {} does not exist", method_enum_to_str(m), endpoint)); 
    }
    return it->second.is_authenticator;
}

std::string http::Endpoint::getResource(Method m) const {
    auto it = methods.find(m);
    if(it == methods.end()) {
        throw http::HTTPException(Method_Not_Allowed,
        std::format("failed to retrieve script: {} {} does not exist", method_enum_to_str(m), endpoint)); 
    }
    return it->second.resource;
}

std::string http::Endpoint::getAccessRole(Method m) const {
    auto it = methods.find(m);
    if(it == methods.end()) {
        throw http::HTTPException(Method_Not_Allowed,
        std::format("failed to retrieve access role: {} {} does not exist", method_enum_to_str(m), endpoint)); 
    }
    return it->second.access_role;
}

http::Handler http::Endpoint::getHandler(Method m) const {
    auto it = methods.find(m);
    if(it == methods.end()) {
        if(m != Get && m !=Head) {
            throw http::HTTPException(http::Method_Not_Allowed, 
            std::format("failed to retrieve handler {} {} does not exist", http::method_enum_to_str(m), endpoint));
        } 
        return assign_handler(m); 
    }
    return it->second.handler;
}

std::vector<Method> http::Endpoint::getMethods() const {
    std::vector<Method> results;
    for(auto m: methods) {
        results.push_back(m.first);
    }
    return results;
}
