#pragma once

#include <string>
#include <string_view>
#include <unordered_map>

#include "http.h"

#include "http/forward.h"

namespace http {

class Request {
public:
    Request& AddHeader(std::string key, std::string value) {
        headers[key] = value;
        return *this;
    }

    std::string GetHeader(std::string key) {
        const auto it = headers.find(key);
        if (it == headers.end()) {
            return "";
        }
        return it->second;
    }

    Method method;
    std::string_view query;
    std::string_view args;
    std::string endpoint_url;
    const http::Endpoint* endpoint{nullptr};
    const http::EndpointMethod* route{nullptr};
    Headers headers;
    std::string_view body;
};

}