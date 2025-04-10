#include "http.h" 

std::string url_decode(std::string&& buf) {
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

std::string http::get_time_stamp() {
    std::ostringstream oss;
    auto now = std::time(nullptr);
    std::tm tm = *std::gmtime(&now);
    oss << std::put_time(&tm, "%a, %d %b %Y %H:%M:%S GMT");
    return oss.str();
}

std::string http::method_enum_to_str(method m) {
    switch (m) {
        case method::Get: return "GET";
        case method::Head: return "HEAD";
        case method::Post: return "POST";
        case method::Options: return "OPTIONS";
        default: return "";
    }
}

http::method http::method_str_to_enum(const std::string& method_str) {
    if(method_str == "GET" || method_str == "get") {
        return http::method::Get;
    } else if (method_str == "POST" || method_str == "post") {
        return http::method::Post;
    } else if (method_str == "HEAD" || method_str == "head") {
        return http::method::Head;
    } else if (method_str == "OPTIONS" || method_str == "options") {
        return http::method::Options;
    } else {
        return http::method::Not_Allowed;
    }
} 

std::string http::trim_to_lower(std::string_view& str_param) {
    size_t first = str_param.find_first_not_of(" \t\n\r");
    if (first == std::string_view::npos) 
        return ""; 
        
    size_t last = str_param.find_last_not_of(" \t\n\r");
    std::string result = std::string(str_param.substr(first, (last - first + 1)));
    std::transform(result.begin(), result.end(), result.begin(), ::tolower);
    return result;
}

std::string http::trim_to_upper(std::string_view& str_param) {
    size_t first = str_param.find_first_not_of(" \t\n\r");
    if (first == std::string_view::npos) 
        return ""; 
        
    size_t last = str_param.find_last_not_of(" \t\n\r");
    std::string result = std::string(str_param.substr(first, (last - first + 1)));
    std::transform(result.begin(), result.end(), result.begin(), ::toupper);
    return result;
}

bool http::is_success_code(http::code status) noexcept {
    return (status >= http::code::OK && status < http::code::Bad_Request);
}

http::code http::determine_content_type(const std::string& resource, std::string& content_type) {
    std::size_t start = resource.rfind('.');
    if (start == std::string::npos) {
        return http::code::Forbidden;
    }

    std::string extension = resource.substr(start);
    for (const auto &ext : http::FILE_EXTENSIONS) {
        if (extension == ext.first) {
            content_type = ext.second;
            return http::code::OK;
        }
    }
    return http::code::Forbidden;
}

std::string http::get_status_msg(http::code http_code) {
    switch (http_code) {
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
    case http::code::See_Other:
        return "HTTP/1.1 303 See Other";
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
    case http::code::Client_Closed_Request:
        return "HTTP/1.1 499 Client Closed Request";
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

std::string http::extract_header_line(const std::vector<char>& buffer) {
    std::string_view response(buffer.data(), buffer.size());
    std::size_t end;
    if((end = response.find("\r\n")) == std::string::npos) {
        return "";
    }
    return (std::string)(response.substr(0, end));
}


// http::code http::extract_endpoint_and_query_str(const std::vector<char>& buffer, std::string& resource, std::string* query) {
//     std::size_t start, end;
//     std::string_view request(buffer.begin(), buffer.end());
//     if ((start = request.find(" ")) == std::string::npos ||
//         (end = request.find(" ", start + 1)) == std::string::npos) {
//         return http::code::Bad_Request;
//     }
//     resource = request.substr(start + 1, end - start - 1);
    
//     if (resource == "/") {
//         resource = "/index.html";
//     }
    
//     if (!resource.empty() && resource[0] == '/') {
//         resource = resource.substr(1);
//     }

//     std::size_t query_pos = resource.find('?');
//     if (query_pos != std::string::npos) {
//         if (query != nullptr) {
//             *query = resource.substr(query_pos + 1);
//         }
//         resource = resource.substr(0, query_pos);
//     }
//     return http::code::OK;
// }

void http::extract_endpoint_and_query_str(const std::vector<char>& buffer, std::string& resource, std::string* query) {
    std::size_t start, end;
    std::string_view request(buffer.begin(), buffer.end());
    if ((start = request.find(" ")) == std::string::npos ||
        (end = request.find(" ", start + 1)) == std::string::npos) {
        throw http::HTTPException(http::code::Bad_Request, "failed to extract endpoint url from request buffer");
        // return http::code::Bad_Request;
    }
    resource = request.substr(start + 1, end - start - 1);
    
    if (resource == "/") {
        resource = "/index.html";
    }
    
    if (!resource.empty() && resource[0] == '/') {
        resource = resource.substr(1);
    }

    std::size_t query_pos = resource.find('?');
    if (query_pos != std::string::npos) {
        if (query != nullptr) {
            *query = resource.substr(query_pos + 1);
        }
        resource = resource.substr(0, query_pos);
    }
    // return http::code::OK;
}


// http::code http::extract_body(const std::vector<char>& buffer, std::string& body) {
//     std::string_view header(buffer.data(), buffer.size());

//     std::size_t start;
//     if ((start = header.find("\r\n\r\n")) == std::string::npos && (start = header.find("\n\n")) == std::string::npos) {
//         return http::code::Bad_Request;    
//     }
//     std::size_t offset = (header[start] == '\r') ? 4 : 2;

//     std::size_t end;
//     if ((end = header.find("\r\n", start + offset)) == std::string::npos && (end = header.find("\n", start + offset)) == std::string::npos) {
//         end = header.size();
//     }

//     body = std::string(header.substr(start + offset, end - (start + offset)));
//     return http::code::OK;
// }



std::string http::extract_body(const std::vector<char>& buffer) {
    std::string_view header(buffer.data(), buffer.size());

    std::size_t start;
    if ((start = header.find("\r\n\r\n")) == std::string::npos && (start = header.find("\n\n")) == std::string::npos) {
        throw http::HTTPException(http::code::Bad_Request, "Failed to extract body from buffer");
        // return http::code::Bad_Request;    
    }
    std::size_t offset = (header[start] == '\r') ? 4 : 2;

    std::size_t end;
    if ((end = header.find("\r\n", start + offset)) == std::string::npos && (end = header.find("\n", start + offset)) == std::string::npos) {
        end = header.size();
    }

    return std::string(header.substr(start + offset, end - (start + offset)));
    // return http::code::OK;
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

    std::string_view content_type_view = std::string_view(header.substr(start, end - start));
    content_type = http::trim_to_lower(content_type_view);
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

    body = http::extract_body(buffer);
    if(/*(code = http::extract_body(buffer, body)) != http::code::OK ||*/ (code = http::find_content_type(buffer, content_type)) != http::code::OK) {
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

http::code http::extract_header_field(const std::vector<char>& buffer, std::string field, std::string& result) {
    field += ": ";
    std::string_view header(buffer.data(), buffer.size());
    std::size_t start, end;
    if((start = header.find(field)) == std::string::npos || (end = header.find("\r\n", start)) == std::string::npos || (start += field.size()) > end) {
        return http::code::Bad_Request;
    }
    result = header.substr(start, end - start);
    return http::code::OK;
}

// http::code http::extract_method(const std::vector<char>& buffer, std::string& method) {
//     std::string_view header(buffer.data(), buffer.size());
//     std::size_t line_end = header.find("\r\n");
//     if (line_end == std::string_view::npos) {
//         return http::code::Bad_Request;
//     }
//     std::string_view request_line = header.substr(0, line_end);

//     std::size_t method_end = request_line.find(' ');
//     if (method_end == std::string_view::npos) {
//         return http::code::Bad_Request;
//     }
//     method = std::string(request_line.substr(0, method_end));

//     if (method.empty()) {
//         return http::code::Bad_Request;
//     }
//     return http::code::OK;
// }

 http::method http::extract_method(const std::vector<char>& buffer) {
    std::string_view header(buffer.data(), buffer.size());
    std::size_t line_end = header.find("\r\n");
    if (line_end == std::string_view::npos) {
        throw http::HTTPException(http::code::Bad_Request, "failed to find return line feed while parsing request method");
    }
    std::string_view request_line = header.substr(0, line_end);

    std::size_t method_end = request_line.find(' ');
    if (method_end == std::string_view::npos) {
        throw http::HTTPException(http::code::Bad_Request, "failed to parse request method");
    }
    return http::method_str_to_enum(std::string(request_line.substr(0, method_end)));

    // if (method.empty()) {
    //     throw http::HTTPException(http::code::Bad_Request, "failed to parse request method");
    // }
    // return http::code::OK;
}


// http::code http::extract_headers(const std::vector<char>& buffer, std::unordered_map<std::string, std::string>& headers) {
//     std::string_view request(buffer.data(), buffer.size());
//     const std::string_view line_end = "\r\n";
//     const std::string_view header_splitter = ": ";

//     std::size_t headers_end = request.find("\r\n\r\n");
//     if (headers_end == std::string_view::npos) {
//         return http::code::Bad_Request;
//     }

//     std::size_t pos = 0;
//     std::size_t end_of_request_line = request.find(line_end, pos);
//     if (end_of_request_line == std::string_view::npos) {
//         return http::code::Bad_Request;
//     }
    
//     pos = end_of_request_line + line_end.size();
//     if (pos >= headers_end) {
//         return http::code::OK; 
//     }

//     while (true) {
//         if (pos >= headers_end) {
//             break;
//         }
//         std::size_t end = request.find(line_end, pos);
//         if (end == std::string_view::npos) {
//             return http::code::Bad_Request;
//         }

//         std::string_view line = request.substr(pos, end - pos);
//         if (line.empty()) {
//             break;
//         }

//         std::size_t splitter_pos = line.find(header_splitter);
//         if (splitter_pos == std::string_view::npos) {
//             return http::code::Bad_Request;
//         }

//         std::string key = std::string(line.substr(0, splitter_pos));
//         std::string value = std::string(line.substr(splitter_pos + header_splitter.size()));

//         headers[std::move(key)] = std::move(value);
//         pos = end + line_end.size();
//     }
//     return http::code::OK;
// }


std::unordered_map<std::string, std::string> http::extract_headers(const std::vector<char>& buffer) {
    std::unordered_map<std::string, std::string> headers;
    std::string_view request(buffer.data(), buffer.size());
    const std::string_view line_end = "\r\n";
    const std::string_view header_splitter = ": ";

    std::size_t headers_end = request.find("\r\n\r\n");
    if (headers_end == std::string_view::npos) {
        throw http::HTTPException(http::code::Bad_Request, "failed to extract headers from request buffer");
        // return http::code::Bad_Request;
    }

    std::size_t pos = 0;
    std::size_t end_of_request_line = request.find(line_end, pos);
    if (end_of_request_line == std::string_view::npos) {
        throw http::HTTPException(http::code::Bad_Request, "failed to extract headers from request buffer");
        // return http::code::Bad_Request;
    }
    
    pos = end_of_request_line + line_end.size();
    if (pos >= headers_end) {
        return headers;
        // return http::code::OK; 
    }

    while (true) {
        if (pos >= headers_end) {
            break;
        }
        std::size_t end = request.find(line_end, pos);
        if (end == std::string_view::npos) {
            throw http::HTTPException(http::code::Bad_Request, "failed to extract headers from request buffer");
            // return http::code::Bad_Request;
        }

        std::string_view line = request.substr(pos, end - pos);
        if (line.empty()) {
            break;
        }

        std::size_t splitter_pos = line.find(header_splitter);
        if (splitter_pos == std::string_view::npos) {
            throw http::HTTPException(http::code::Bad_Request, "failed to extract headers from request buffer");
            // return http::code::Bad_Request;
        }

        std::string key = std::string(line.substr(0, splitter_pos));
        std::string value = std::string(line.substr(splitter_pos + header_splitter.size()));

        headers[std::move(key)] = std::move(value);
        pos = end + line_end.size();
    }
    return headers;
    // return http::code::OK;
}

std::string http::extract_jwt_from_cookie(const std::string& cookie) {
    const std::string jwt_key = "jwt=";
    size_t start = cookie.find(jwt_key);
    if (start == std::string::npos) {
        return "";
    }
    start += jwt_key.size(); 
    size_t end = cookie.find(";", start);
    return cookie.substr(start, end - start); 
}

http::code http::extract_status_code(const std::vector<char>& buffer) {
    std::string_view response(buffer.begin(), buffer.end());

    size_t version_end = response.find(' ');
    if (version_end == std::string::npos) {
        return http::code::Bad_Request;
    }

    size_t status_code_start = version_end + 1;
    size_t status_code_end = response.find(' ', status_code_start);
    if (status_code_end == std::string::npos) {
        return http::code::Bad_Gateway;
    }

    std::string status_code_str = (std::string)response.substr(status_code_start, status_code_end - status_code_start);
    try {
        return static_cast<http::code>(std::stoi(status_code_str)); 
    } catch (const std::exception&) {
        return http::code::Bad_Gateway;
    }
}

http::code http::extract_token(const std::vector<char>& buffer, std::string& token) {
    
    std::string_view header(buffer.data(), buffer.size());
    http::code code;
    std::string field;
    if((code = http::extract_header_field(buffer, "Authorization", field)) != http::code::OK) {
        return code;
    }

    std::string delim = "Bearer ";
    std::size_t start;
    if((start = field.find(delim)) == std::string::npos) {
        return http::code::Bad_Request;
    }

    token = field.substr(delim.length());
    return http::code::OK;
}
