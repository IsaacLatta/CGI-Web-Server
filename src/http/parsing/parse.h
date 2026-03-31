#pragma once

#include <string>
#include <vector>
#include <utility>
#include <unordered_map>
#include <span>
#include <random>

#include <nlohmann/json.hpp>
#include <jwt-cpp/jwt.h>

#include "io/io.h"

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

    bool is_success_code(Code status) noexcept;

    Method extract_method(std::span<const char> buffer);
    Code extract_token(const std::vector<char>& buffer, std::string& token);
    std::unordered_map<std::string, std::string> extract_headers(std::span<const char> buffer);
    Code extract_status_code(std::span<const char> buffer) noexcept;
    std::optional<std::string> extract_jwt_from_cookie(const std::string& cookie);

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
    std::string_view extract_args(std::span<const char> buffer, http::ArgumentType arg);

    ArgumentType arg_str_to_enum(const std::string& args_str) noexcept;

    QueryParams extract_query_params(std::span<const char>);

    inline http::Code error_to_status(const asio::error_code& ec) noexcept {
        if (!ec) {
            return OK;
        } else if (io::is_client_disconnect(ec)) {
            return Client_Closed_Request;
        } else if (ec == asio::error::access_denied) {
            return Forbidden;
        } else if (io::is_permanent_failure(ec)) {
            return Internal_Server_Error;
        } else if (ec == asio::error::no_buffer_space) {
            return Insufficient_Storage;
        } else if (ec == asio::error::timed_out) {
            return Gateway_Timeout;
        }
        return Service_Unavailable;
    }

    namespace detail {
        Handler assign_handler(Method);
    }

} //namespace http
