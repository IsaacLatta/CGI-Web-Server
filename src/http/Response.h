#pragma once

#include <string>
#include <unordered_map>

#include "forward.h"
#include "http.h"

namespace http {
    class Response {
    public:
        Code Status { OK };

    public:
        Response() = default;

        explicit Response(Code new_status) {
            SetStatus(new_status);
        }

        Response& SetBody(std::string body) {
            body_ = std::move(body);
            return *this;
        }

        const std::string& GetBody() const {
            return body_;
        }

        Response& SetStatus(Code new_status) {
            Status = new_status;
            return *this;
        }

        Response& AddHeaders(const Headers& headers) {
            for (auto& [k,v] : headers) {
                this->headers_[k] = v;
            }
            return *this;
        }

        Response& AddHeader(std::string key, const std::string& val) {
            headers_[key] = val;
            return *this;
        }

        // TODO: Figure out wtf is going
        // on with these two methods, is
        // there some weird dependency on this
        // behavior somewhere?

        const std::string& AsString() const {
            return built_response_;
        }

        std::string Build() {
            built_response_ = std::string(get_status_msg(Status)) + "\r\n";

            headers_["Date"] = get_time_stamp();
            for(auto& [key, value] : headers_) {
                built_response_ += key + ": " + value + "\r\n";
            }
            return built_response_ + "\r\n" + body_;
        }

    private:
        std::string built_response_;
        std::string body_;
        Headers headers_;
    };
}
