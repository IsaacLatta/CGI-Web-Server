    #ifndef HTTP_H
#define HTTP_H


#include <string>
#include <vector>
#include <iostream>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <memory>
#include <cmath>
#include <string>
#include <utility>
#include <unordered_map>
#include <span>
#include <cctype>

#include "json.hpp"
#include <jwt-cpp/jwt.h>

#include "config.h"
#include "Router.h"

namespace http
{
    using json = nlohmann::json;

    enum class code {
        Not_A_Status = -1,
        OK = 200,
        Created = 201,
        Accepted = 202,
        No_Content = 204,
        Moved_Permanently = 301,
        Found = 302,
        See_Other = 303,
        Not_Modified = 304,
        Bad_Request = 400,
        Unauthorized = 401,
        Forbidden = 403,
        Not_Found = 404,
        Method_Not_Allowed = 405,
        Client_Closed_Request = 499,
        Internal_Server_Error = 500,
        Not_Implemented = 501,
        Bad_Gateway = 502,
        Service_Unavailable = 503 
    };

    code code_str_to_enum(const char* code_str);

    enum class method : int {
        Get,
        Head,
        Post,
        Options,
        Not_Allowed
    };

    method method_str_to_enum(const std::string& method_str);
    std::string_view method_enum_to_str(method m);

    const std::vector<std::pair<std::string, std::string>> FILE_EXTENSIONS = {
        // HTML, CSS, JS
        {".html", "text/html"},
        {".css",  "text/css"},
        {".js",   "application/javascript"},

        // Images
        {".png",  "image/png"},
        {".jpg",  "image/jpeg"},
        {".gif",  "image/gif"},
        {".ico",  "image/ico"},
        {".svg",  "image/svg+xml"},

        // Documents / other
        {".pdf",  "application/pdf"},
        {".txt",  "text/plain"},

        // Fonts
        {".woff",  "font/woff"},
        {".woff2", "font/woff2"},
        {".ttf",   "font/ttf"},
        {".otf",   "font/otf"}
    };

    std::string_view get_status_msg(code http_code);
    std::string get_time_stamp();

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

        void addHeaders(const std::unordered_map<std::string, std::string>& headers) {
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

        http::code getStatus() const {return status;}
    };

    class HTTPException : public std::exception {
        public:

            HTTPException() {}

            HTTPException(code status, std::string&& message): response(status), message(std::move(message)) {
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

    struct Request {
        http::method method;
        std::string_view args;
        std::string endpoint_url;
        const http::Endpoint* route{nullptr}; 
        std::unordered_map<std::string, std::string> headers;
        std::string_view body;

        void addHeader(std::string key, std::string value) {headers[key] = value;}

        std::string getHeader(std::string key) {
            auto it = headers.find(key);
            if(it == headers.end()) {
                return "";
            }
            return it->second;
        }
    };

    bool is_success_code(http::code status) noexcept;

    method extract_method(std::span<const char> buffer);
    code extract_token(const std::vector<char>& buffer, std::string& token);
    std::unordered_map<std::string, std::string> extract_headers(std::span<const char> buffer);
    code extract_status_code(std::span<const char> buffer) noexcept;
    std::string extract_jwt_from_cookie(const std::string& cookie);

    std::string trim_to_lower(std::string_view& str);
    std::string trim_to_upper(std::string_view& str);

    code build_json(std::span<const char> buffer, json& json_array);
    json parse_url_form(const std::string& body);
    std::string_view extract_header_line(std::span<const char> buffer);
    std::string_view extract_body(std::span<const char> buffer);
    code find_content_type(std::span<const char> buffer, std::string& content_type) noexcept;
    std::string_view get_request_target(std::span<const char> buffer);
    std::string extract_resource(std::span<const char> buffer);
    std::string_view extract_query_string(std::span<const char> buffer);
    code determine_content_type(const std::string& resource, std::string& content_type);
    std::string_view extract_args(std::span<const char> buffer, http::arg_type arg);
};

#endif