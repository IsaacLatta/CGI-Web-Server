#ifndef Transaction_H
#define Transaction_H

#include <string>
#include <vector>
#include <asio.hpp>
#include <optional>

#include "Socket.h"
#include "http.h"
#include "logger.h"

class Transaction
{
    public:
    int fd;
    long content_len;
    std::size_t status;
    std::string resp_header;
    std::string req_header;
    std::string user_agent;
    std::string resource;
    std::string content_type;
    std::vector<char> response;

    Transaction() : status(OK), fd(-1) {};
    void process(const std::vector<char>& buffer);
    bool error(const std::optional<asio::error_code> error = std::nullopt);
    ~Transaction();
    private:
    void open_resource();
    void build_response();
};


#endif