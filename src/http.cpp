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

std::string trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\n\r");
    if (first == std::string::npos) return "";
    size_t last = str.find_last_not_of(" \t\n\r");
    return str.substr(first, (last - first + 1));
}


static http::code check_ext(const std::string& extension, std::string& content_type)
{
    std::vector<std::pair<std::string, std::string>> valid_exts = { {".html", "text/html"}, {".css", "text/css"}, 
        {".js", "application/javascript"}, {".png", "image/png"}, {".jpg", "image/jpeg"}, {".ico", "image/ico"},
        {".gif", "image/gif"}, {".pdf", "application/pdf"}, {".txt", "text/plain"} };
    
    for(auto& ext : valid_exts)
    {
        if(extension == ext.first)
        {
            content_type = ext.second;
            return http::code::OK;
        }
    }
    return http::code::Forbidden;
}

std::string http::get_response(http::code http_code)
{
    switch (http_code)
    {
    case http::code::OK:
        return "HTTP/1.1 200 OK";
    case http::code::Created:
        return "HTTP/1.1 201 Created";
    case http::code::Accepted:
        return "HTTP/1.1 202 Accepted";
    case http::code::No_Content:
        return "HTTP/1.1 204 No Content";
    case http::code::Moved_Permanently:
        return "HTTP/1.1 301 Moved Permanently";
    case http::code::Found:
        return "HTTP/1.1 302 Found";
    case http::code::Not_Modified:
        return "HTTP/1.1 304 Not Modified";
    case http::code::Bad_Request:
        return "HTTP/1.1 400 Bad Request";
    case http::code::Unauthorized:
        return "HTTP/1.1 401 Unauthorized";
    case http::code::Forbidden:
        return "HTTP/1.1 403 Forbidden";
    case http::code::Not_Found: 
        return "HTTP/1.1 404 Not Found";
    case http::code::Method_Not_Allowed:
        return "HTTP/1.1 405 Method Not Allowed";
    case http::code::Internal_Server_Error:
        return "HTTP/1.1 500 Internal Server Error";
    case http::code::Not_Implemented:
        return "HTTP/1.1 501 Not Implemented";
    case http::code::Bad_Gateway:
        return "HTTP/1.1 502 Bad Gateway";
    case http::code::Service_Unavailable:
        return "HTTP/1.1 503 Service Unavailable";
    default:
        return "HTTP/1.1 500 Internal Server Error"; 
    }
}

http::code http::determine_content_type(const std::string& resource, std::string& content_type)
{
    std::size_t start;
    std::string extension;
    if((start = resource.find(".")) == std::string::npos)
    {
        return http::code::Forbidden;
    }
    extension = resource.substr(start, resource.length() - start);

    return check_ext(extension, content_type);
}

/*
POST /resource HTTP/1.1
Host: 127.0.0.1:1025
User-Agent: curl/8.6.0
Accept: 
Content-Length: 23
Content-Type: application/x-www-form-urlencoded

key1=value1&key2=value2�T
*/

http::code http::extract_endpoint(const std::vector<char>& buffer, std::string& resource)
{
    std::size_t start, end;
    std::string_view request(buffer.begin(), buffer.end());
    if((start = request.find(" ")) == std::string::npos || (end = request.find(" ", start + 1)) == std::string::npos) {
        return http::code::Bad_Request;
    }
    resource = request.substr(start + 1, end - start - 1);
    if(resource == "/") {
        resource = "/index.html";
    }
    return http::code::OK;
}

http::code http::extract_body(const std::vector<char>& buffer, std::string& body) {
    std::string_view header(buffer.data(), buffer.size());

    std::size_t start;
    if ((start = header.find("\r\n\r\n")) == std::string::npos && (start = header.find("\n\n")) == std::string::npos) {
        return http::code::Bad_Request;    
    }
    std::size_t offset = (header[start] == '\r') ? 4 : 2;

    std::size_t end;
    if ((end = header.find("\r\n", start + offset)) == std::string::npos && (end = header.find("\n", start + offset)) == std::string::npos) {
        end = header.size();
    }

    body = std::string(header.substr(start + offset, end - (start + offset)));
    return http::code::OK;
}


http::code http::find_content_type(const std::vector<char>& buffer, std::string& content_type) {
    std::string_view header(buffer.data(), buffer.size());
    const std::string delimiter = "Content-Type: ";

    std::size_t start = header.find(delimiter);
    if (start == std::string::npos) {
        content_type = "application/json";
        return http::code::OK;
    }
    start += delimiter.length();

    std::size_t end;
    if ((end = header.find("\r\n", start)) == std::string::npos && (end = header.find("\n", start)) == std::string::npos) {
        return http::code::Bad_Request; 
    }

    content_type = std::string(header.substr(start, end - start));
    trim(content_type);
    std::transform(content_type.begin(), content_type.end(), content_type.begin(), ::tolower);

    return http::code::OK;
}

http::json http::parse_url_form(const std::string& body) {
    std::istringstream stream(body);
    std::string key_val_pair;

    http::json result;
    while (std::getline(stream, key_val_pair, '&')) {
        size_t delimiter_pos = key_val_pair.find('=');
        if (delimiter_pos != std::string::npos) {
            std::string key = url_decode(key_val_pair.substr(0, delimiter_pos));
            std::string value = url_decode(key_val_pair.substr(delimiter_pos + 1));
            result[key] = value;
        }
    }
    return result;
}

http::code http::build_json(const std::vector<char>& buffer, http::json& json_array) {
    http::code code;
    std::string body, content_type;
    if((code = http::extract_body(buffer, body)) != http::code::OK || (code = http::find_content_type(buffer, content_type)) != http::code::OK) {
        return code;
    }

    if(content_type == "application/x-www-form-urlencoded") {
        json_array = http::parse_url_form(body);
    }
    else if (content_type == "application/json") {
        json_array = http::json::parse(body);
    }
    else {
        return http::code::Not_Implemented;
    }

    return http::code::OK;
}
