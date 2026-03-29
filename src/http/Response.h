#pragma once

#include <string>
#include <unordered_map>

#include "forward.h"
#include "http.h"

namespace http {

    class Response {
    public:
        code status{code::OK};
        std::string status_msg{get_status_msg(code::OK)};
        std::string body{""};
        std::unordered_map<std::string, std::string> headers;
        std::string built_response{""};

        Response() {}
        Response(code new_status) {setStatus(new_status);}

        void setStatus(code new_status) {
            status = new_status;
            status_msg = http::get_status_msg(status);
        }

        void addHeaders(const Headers& headers) {
            for (auto& [k,v] : headers) {
                this->headers[k] = v;
            }
        }

        void addHeader(std::string key, const std::string& val) {
            headers[key] = val;
        }

        std::string getStr() const {
            return built_response;
        }

        std::string build() {
            built_response = status_msg + "\r\n";

            headers["Date"] = get_time_stamp();
            for(auto& [key, value] : headers) {
                built_response += key + ": " + value + "\r\n";
            }
            return built_response + "\r\n" + body;
        }

        code getStatus() const {
            return status;
        }
    };

}
