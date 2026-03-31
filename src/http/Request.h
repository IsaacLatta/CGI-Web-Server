#pragma once

#include <string>
#include <string_view>
#include <unordered_map>

#include "parsing/parse.h"

#include "http/forward.h"
#include "routing/Route.h"

namespace http {

class Request {
public:
    const Headers& GetHeaders() {
        return headers_;
    }

    std::string_view GetQueryString() const noexcept {
        return query_string_;
    }

    Request& SetQueryString(std::string_view query) {
        query_string_ = query;
        return *this;
    }

    Request& SetQueryParams(QueryParams params) {
        params_ = std::move(params);
        return *this;
    }

    Request& SetHeaders(Headers headers) {
        headers_ = std::move(headers);
        return *this;
    }

    Request& SetPath(std::string path) {
        path_ = std::move(path);
        return *this;
    }

    std::string GetPath() const noexcept {
        return path_;
    }

    Request& AddHeaders(const Headers& headers) {
        headers_ = headers;
        return *this;
    }

    Request& AddHeader(std::string key, std::string value) {
        headers_[key] = value;
        return *this;
    }

    std::string GetHeader(std::string key) const {
        const auto it = headers_.find(key);
        if (it == headers_.end()) {
            return "";
        }
        return it->second;
    }

    Request& SetMethod(Method m) {
        method_ = m;
        return *this;
    }

    Request& SetBody(std::string_view body) {
        body_ = body;
        return *this;
    }

    std::string_view GetBody() const noexcept {
        return body_;
    }

    Method GetMethod() const noexcept {
        return method_;
    }

private:
    std::string_view body_;
    QueryParams params_;
    std::string_view query_string_;
    Method method_ { Unassigned };
    Headers headers_;
    std::string path_;
};

}
