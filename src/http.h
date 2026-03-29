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

#include <nlohmann/json.hpp>
#include <jwt-cpp/jwt.h>

#include "config.h"
#include "Router.h"
#include "io/Socket.h"

#include "http/forward.h"

namespace http {
    using json = nlohmann::json;

    Code code_str_to_enum(const char* code_str);

    Method method_str_to_enum(const std::string& method_str);
    std::string_view method_enum_to_str(Method m);

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

    std::string_view get_status_msg(Code http_code);
    std::string get_time_stamp();

    bool is_success_code(http::Code status) noexcept;

    Method extract_method(std::span<const char> buffer);
    Code extract_token(const std::vector<char>& buffer, std::string& token);
    std::unordered_map<std::string, std::string> extract_headers(std::span<const char> buffer);
    Code extract_status_code(std::span<const char> buffer) noexcept;
    std::string extract_jwt_from_cookie(const std::string& cookie);

    std::string trim_to_lower(std::string_view& str);
    std::string trim_to_upper(std::string_view& str);

    Code build_json(std::span<const char> buffer, json& json_array);
    json parse_url_form(const std::string& body);
    std::string_view extract_header_line(std::span<const char> buffer);
    std::string_view extract_body(std::span<const char> buffer);
    Code find_content_type(std::span<const char> buffer, std::string& content_type) noexcept;
    std::string_view get_request_target(std::span<const char> buffer);
    std::string extract_endpoint(std::span<const char> buffer);
    std::string_view extract_query_string(std::span<const char> buffer);
    Code determine_content_type(const std::string& resource, std::string& content_type);
    std::string_view extract_args(std::span<const char> buffer, http::arg_type arg);

    struct WriteStatus {
        http::Code status{Code::OK};
        std::string message{"Success"};
        std::size_t bytes{0};
    };

    asio::awaitable<WriteStatus> co_write_all(::io::Socket* sock, std::span<const char> buffer) noexcept;
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

    inline http::Code error_to_status(const asio::error_code& ec) noexcept {
        if (!ec) {
            return http::Code::OK;
        } else if (is_client_disconnect(ec)) {
            return http::Code::Client_Closed_Request;
        } else if (ec == asio::error::access_denied) {
            return http::Code::Forbidden;
        } else if (is_permanent_failure(ec)) {
            return http::Code::Internal_Server_Error;
        } else if (ec == asio::error::no_buffer_space) {
            return http::Code::Insufficient_Storage;
        } else if (ec == asio::error::timed_out) {
            return http::Code::Gateway_Timeout;
        }
        return http::Code::Service_Unavailable;
    }
} //namespace http

#endif