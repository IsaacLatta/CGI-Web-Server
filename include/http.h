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


namespace http
{
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
        Internal_Server_Error = 500,
        Not_Implemented = 501,
        Bad_Gateway = 502,
        Service_Unavailable = 503 
    };

    code validate_method(const std::vector<char>& buffer);
    code validate_buffer(const std::vector<char>& buffer);
    code extract_resource(const std::vector<char>& buffer, std::string& resource);
    code extract_content_type(const std::string& resource, std::string& content_type);
    void clean_buffer(std::vector<char>& buffer);
};

#endif