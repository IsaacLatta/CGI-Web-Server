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

#define ERROR 86
#define SUCCESS 23
#define OK 200
#define BAD_REQUEST 400
#define FORBIDDEN 403
#define NOT_FOUND 404
#define INTERNAL_SERVER_ERROR 500

namespace http
{
    std::size_t validate_method(const std::vector<char>& buffer);
    std::size_t validate_buffer(const std::vector<char>& buffer);
    std::size_t extract_resource(const std::vector<char>& buffer, std::string& resource);
    std::size_t extract_content_type(const std::string& resource, std::string& content_type);
    void clean_buffer(std::vector<char>& buffer);
};

#endif