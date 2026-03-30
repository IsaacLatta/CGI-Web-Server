#pragma once

#include <exception>

#include "http.h"
#include "http/Response.h"

namespace http {

    class Exception : public std::exception {
    public:
        Exception() = default;

        Exception(Code status) : Exception(status, {}) {}

        Exception(Code status, std::string&& message): Exception(status, std::move(message), {}) {}

        Exception(Code status, std::string&& message, Headers&& headers): message_(std::move(message)) {
            response_.SetStatus(status)
                    .AddHeaders(headers)
                    .AddHeader("Connection", "close")
                    .AddHeader("Content-Length", "0")
                    .Build();
        }

        const Response& GetResponse() const {
            return response_;
        }

        const char* what() const noexcept override {
            if (!message_.empty()) {
                return message_.c_str();
            }
            return get_status_msg(response_.Status).data();
        }

    private:
        Response response_;
        std::string message_;
    };

}
