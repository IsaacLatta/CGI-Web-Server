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
#include "json.hpp"
#include <jwt-cpp/jwt.h>

namespace http
{
    using json = nlohmann::json;

    enum class code {
        OK = 200,
        Created = 201,
        Accepted = 202,
        No_Content = 204,
        Moved_Permanently = 301,
        Found = 302,
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

    std::string get_status_msg(code http_code);

    struct error {
        std::string message;
        std::string response;
        code error_code;
        error() {}
        error(code ec, std::string&& message = ""): message(message), error_code(ec) {response = get_status_msg(ec);}
    };

    // Optionally add a file descriptor for body
    struct Response {
        code status{code::OK};
        std::string status_msg{get_status_msg(code::OK)};
        std::unordered_map<std::string, std::string> headers;
        std::string built_response{""};

        void setStatus(code new_status) {
            status = new_status;
            status_msg = http::get_status_msg(status);
        }

        void addHeader(const std::string& key, const std::string& val) {
            headers[key] = val;
        }

        std::string build() {
            built_response = status_msg + "\r\n";
            for(auto& [key, value] : headers) {
                built_response += key + ": " + value + "\r\n";
            }
            built_response = built_response + "\r\n\r\n";
            return built_response;
        }
    };


    //code verify_token(const std::vector<char>& buffer, const std::string& role);

    code extract_token(const std::vector<char>& buffer, std::string& token);
    code extract_header_field(const std::vector<char>& buffer, std::string field, std::string& result);
    
    std::string trim_to_lower(const std::string& str);
    code build_json(const std::vector<char>& buffer, json& json_array);
    json parse_url_form(const std::string& body);
    std::string extract_header_line(const std::vector<char>& buffer);
    code extract_body(const std::vector<char>& buffer, std::string& body);
    code find_content_type(const std::vector<char>& buffer, std::string& content_type);
    code extract_endpoint(const std::vector<char>& buffer, std::string& resource);
    code determine_content_type(const std::string& resource, std::string& content_type);
    void clean_buffer(std::vector<char>& buffer);

};

#endif