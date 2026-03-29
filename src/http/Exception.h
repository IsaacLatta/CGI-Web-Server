#pragma once

#include <exception>

#include "http.h"
#include "http/Response.h"

namespace http {

    class HTTPException : public std::exception {
    public:
        HTTPException() = default;

        HTTPException(Code status, std::string&& message): message_(std::move(message)) {
            response_.SetStatus(status)
                    .AddHeader("Connection", "close")
                    .AddHeader("Content-Length", "0")
                    .Build();
        }

        HTTPException(Code status, std::string&& message, Headers&& headers): message_(std::move(message)) {
            response_.SetStatus(status)
                    .AddHeaders(headers)
                    .AddHeader("Connection", "close")
                    .AddHeader("Content-Length", "0")
                    .Build();
        }

        const Response* getResponse() const {
            return &response_;
        }

        const char* what() const noexcept override {
            return message_.c_str();
        }
    private:
        Response response_;
        std::string message_;
    };

}
