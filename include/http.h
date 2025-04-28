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
#include <chrono>
#include <random>

#include "json.hpp"
#include <jwt-cpp/jwt.h>

#include "config.h"
#include "Router.h"
#include "Socket.h"

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
        Unsupported_Media_Type = 415,
        Too_Many_Requests = 429,
        Client_Closed_Request = 499,
        Internal_Server_Error = 500,
        Not_Implemented = 501,
        Bad_Gateway = 502,
        Service_Unavailable = 503,
        Gateway_Timeout = 504,
        Insufficient_Storage = 507
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
        const http::Endpoint* endpoint{nullptr};
        const http::EndpointMethod* route{nullptr};
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
    std::string extract_endpoint(std::span<const char> buffer);
    std::string_view extract_query_string(std::span<const char> buffer);
    code determine_content_type(const std::string& resource, std::string& content_type);
    std::string_view extract_args(std::span<const char> buffer, http::arg_type arg);

    namespace io {
        struct WriteStatus {
            http::code status{code::OK};
            std::string message{"Success"};
            std::size_t bytes{0};
        };        

        asio::awaitable<WriteStatus> co_write_all(Socket* sock, std::span<const char> buffer) noexcept;
        std::chrono::milliseconds select_backoff(const asio::error_code& ec, int retry) noexcept; 
        asio::awaitable<void> backoff(const asio::error_code& ec, int retry) noexcept;
        
        inline bool is_client_disconnect(const asio::error_code& ec) noexcept {
            return ec == asio::error::connection_reset || ec == asio::error::broken_pipe || ec == asio::error::eof;
        }

        inline bool is_permanent_failure(const asio::error_code& ec) noexcept {
            return ec == asio::error::bad_descriptor || ec == asio::error::address_in_use;
        }

        inline bool is_retryable(const asio::error_code& ec) noexcept {
            return ec == asio::error::would_block || ec == asio::error::try_again || ec == asio::error::network_unreachable
                || ec == asio::error::host_unreachable || ec == asio::error::connection_refused || ec == asio::error::timed_out
                || ec == asio::error::no_buffer_space;
        }

        inline http::code error_to_status(const asio::error_code& ec) noexcept {
            if (!ec) {
                return http::code::OK;
            } else if (is_client_disconnect(ec)) {
                return http::code::Client_Closed_Request;
            } else if (ec == asio::error::access_denied) {
                return http::code::Forbidden;
            } else if (is_permanent_failure(ec)) {
                return http::code::Internal_Server_Error;
            } else if (ec == asio::error::no_buffer_space) {
                return http::code::Insufficient_Storage;
            } else if (ec == asio::error::timed_out) {
                return http::code::Gateway_Timeout;
            }
            return http::code::Service_Unavailable;
        }
    };
};

#endif