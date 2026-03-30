#pragma once

#include <unordered_map>
#include <string>
#include <mutex>

#include "http/forward.h"

namespace http {

class Router {
public:
    Router(std::unordered_map<std::string, Route> endpoints,
        std::unordered_map<Code, ErrorPage> error_pages,
        EndpointFactory default_endpoint_factory,
        ErrorPageFactory default_error_page_factory)
        : endpoints_(std::move(endpoints)), error_pages_(std::move(error_pages)),
            default_endpoint_factory_(std::move(default_endpoint_factory)),
            default_error_page_factory_(std::move(default_error_page_factory)) {}

    const Route& GetRoute(const std::string& endpoint_url) const;

    const Endpoint& GetEndpointMethod(const std::string& endpoint_url, Method) const;

    const ErrorPage& GetErrorPage(Code) const;

private:
    mutable std::mutex endpoints_mtx_;
    mutable std::unordered_map<std::string, Route> endpoints_;

    mutable std::mutex error_pages_mtx_;
    mutable std::unordered_map<Code, ErrorPage> error_pages_;

    EndpointFactory default_endpoint_factory_;
    ErrorPageFactory default_error_page_factory_;
};

} // namespace http
