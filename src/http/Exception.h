#pragma once

#include <exception>

#include "http.h"
#include "http/Response.h"

namespace http {

    class HTTPException : public std::exception {
    public:
        HTTPException() = default;

        HTTPException(code status, std::string&& message): response(status), message(std::move(message)) {
            response.addHeader("Connection", "close");
            response.addHeader("Content-Length", "0");
            response.build();
        }

        HTTPException(code status, std::string&& message, std::unordered_map<std::string, std::string>&& headers): response(status), message(std::move(message)) {
            response.addHeaders(headers);
            response.addHeader("Connection", "close");
            response.addHeader("Content-Length", "0");
            response.build();
        }

        const Response* getResponse() const {return &response;}

        const char* what() const noexcept override {
            return message.c_str();
        }
    private:
        Response response;
        std::string message;
    };

}
