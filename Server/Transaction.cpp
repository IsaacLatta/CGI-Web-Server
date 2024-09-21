#include "Transaction.h"

void Transaction::build_response()
{
    std::string resp;
    switch(status)
    {
        case OK:
        resp_header = "HTTP/1.1 200 OK\r\nConetent-Length: " + std::to_string(content_len) + "\r\nContent-Type: " + content_type + "\r\nConnection: close\r\n\r\n";
        break;
        case FORBIDDEN:
        resp_header = "HTTP/1.1 403 Forbidden\r\n";
        break;
        case BAD_REQUEST:
        resp_header = "HTTP/1.1 400 Bad Request\r\n";
        break;
        case INTERNAL_SERVER_ERROR:
        resp_header = "HTTP/1.1 500 Internal Server Error\r\n";
        break;
        case NOT_FOUND:
        resp_header = "HTTP/1.1 404 Not Found\r\n";
    }
}

void Transaction::process(const std::vector<char>& buffer)
{
    if((status = http::validate_method(buffer)) != OK || 
       (status = http::validate_buffer(buffer)) != OK ||
       (status = http::extract_resource(buffer, this->resource)) != OK || 
       (status = http::extract_content_type(resource, content_type)) != OK)
    {
        logger::debug("REQUEST ERROR", "buffer validation failed, code", std::to_string(status), __FILE__, __LINE__);
    }
    req_header = logger::get_header(buffer);
    user_agent = logger::get_user_agent(buffer);
    open_resource();
    build_response();
}

bool Transaction::error(const std::optional<asio::error_code> error)
{
    if(this->status != OK)
        return true;

    if(error && *error)
    {
        return true;
    }
    return false;
}

void Transaction::open_resource()
{
    logger::debug("INFO", "opening", resource, __FILE__, __LINE__);
    this->fd = open(this->resource.data(), O_RDONLY);
    if(this->fd < 0)
    {
        logger::debug("ERROR ", "open, Errno", std::to_string(errno), __FILE__, __LINE__);
        this->status = NOT_FOUND;
        return;
    }

    content_len = lseek(fd, (off_t)0, SEEK_END);
    lseek(fd, (off_t)0, SEEK_SET); 
}

Transaction::~Transaction()
{
    if(this->fd >= 0)
    {
        close(this->fd);
    }
}