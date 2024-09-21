#include "http.h" 

std::string url_decode(std::string&& buf)
{
    std::string decoded_buf;
    char hex_code[3] = {'\0'};
    for (int i = 0; i < buf.length(); i++)
    {
        if(buf[i] == '%' && i + 2 < buf.length() && 
        std::isxdigit(buf[i + 1]) && std::isxdigit(buf[i + 2]))
        {
            hex_code[0] = buf[i + 1];
            hex_code[1] = buf[i + 2];
            decoded_buf += static_cast<char>(std::strtol(hex_code, nullptr, 16));
            i += 2;
        }
        else if (buf[i] == '+')
            decoded_buf += ' ';
        else
            decoded_buf += buf[i];
    }
    return decoded_buf;
}

void http::clean_buffer(std::vector<char>& buffer)
{
    for(int i = 0; i < buffer.size(); i++)
    {
        if(buffer[i] == '\r' || buffer[i] == '\n')
        {
            buffer[i] = '*';
        }
    }
}

static std::size_t check_ext(const std::string& extension, std::string& content_type)
{
    std::vector<std::pair<std::string, std::string>> valid_exts = { {".html", "text/html"}, {".css", "text/css"}, 
        {".js", "application/javascript"}, {".png", "image/png"}, {".jpg", "image/jpeg"},
        {".gif", "image/gif"}, {".pdf", "application/pdf"}, {".txt", "text/plain"} };
    
    for(auto& ext : valid_exts)
    {
        if(extension == ext.first)
        {
            content_type = ext.second;
            return OK;
        }
    }
    return FORBIDDEN;
}

std::size_t http::extract_content_type(const std::string& resource, std::string& content_type)
{
    std::size_t start;
    std::string extension;
    if((start = resource.find(".")) == std::string::npos)
    {
        return FORBIDDEN;
    }
    extension = resource.substr(start, resource.length() - start);

    return check_ext(extension, content_type);
}

std::size_t http::extract_resource(const std::vector<char>& buffer, std::string& resource)
{
    std::size_t start, end;
    std::string request(buffer.begin(), buffer.end());
    if((start = request.find(" ")) == std::string::npos || (end = request.find(" ", start + 1)) == std::string::npos)
    {
        return BAD_REQUEST;
    }
    resource = request.substr(start + 1, end - start);
    if(resource == "/")
    {
        resource = "index.html";
    }
    return OK;
}

std::size_t http::validate_method(const std::vector<char>& buffer)
{
    std::size_t pos;
    std::string str(buffer.begin(), buffer.end());
    if((pos = str.find("HTTP/")) == std::string::npos)
    {
        return BAD_REQUEST;
    }
    std::string method = str.substr(0, pos);
    if(method.find("GET") == std::string::npos && method.find("get") == std::string::npos)
    {
        return FORBIDDEN;
    }
    return OK;
}

std::size_t http::validate_buffer(const std::vector<char>& buffer)
{
    std::vector<const char*> bad_strs = { "../", "./", "..\\", "'", "`", "<", ">", "|", "&" };
    std::string request = url_decode(std::string(buffer.begin(), buffer.end()));
    for(auto& str: bad_strs)
    {
        if(request.find(str) != std::string::npos)
        {
            return FORBIDDEN;
        }
    }
    return validate_method(buffer);
}