#include "http/routing/Router.h"
#include "http/routing/Route.h"

namespace http {

const Route& Router::GetRoute(const std::string& endpoint) const {
    std::lock_guard<std::mutex> lk(endpoints_mtx_);
    const auto it = endpoints_.find(endpoint);
    if(it == endpoints_.end()) {
        auto [inserted_it, _] = endpoints_.emplace(endpoint, default_endpoint_factory_(endpoint));
        return inserted_it->second;
    }
    return it->second;
}

const Endpoint& Router::GetEndpointMethod(const std::string& endpoint_url, Method m) const {
    const auto endpoint = GetRoute(endpoint_url);
    return endpoint.GetEndpoint(m);
}

const ErrorPage& Router::GetErrorPage(Code status) const {
    std::lock_guard<std::mutex> lk(error_pages_mtx_);
    const auto it = error_pages_.find(status);
    if (it == error_pages_.end()) {
        auto [inserted_it, _] = error_pages_.emplace(status, default_error_page_factory_(status));
        return inserted_it->second;
    }
    return it->second;
}

}
