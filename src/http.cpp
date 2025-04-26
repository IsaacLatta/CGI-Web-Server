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

http::code http::code_str_to_enum(const char* code_str) {
    if (!code_str) {
        return http::code::Not_A_Status;
    }
    try {
        int numeric_code = std::stoi(code_str);
        return static_cast<http::code>(numeric_code);
    } catch (const std::exception& e) {
        return http::code::Not_A_Status;
    }
}

std::string_view http::method_enum_to_str(method m) {
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

    //     Not_A_Status = -1,
    //     OK = 200,
    //     Created = 201,
    //     Accepted = 202,
    //     No_Content = 204,
    //     Moved_Permanently = 301,
    //     Found = 302,
    //     See_Other = 303,
    //     Not_Modified = 304,
    //     Bad_Request = 400,
    //     Unauthorized = 401,
    //     Forbidden = 403,
    //     Not_Found = 404,
    //     Method_Not_Allowed = 405,
    //     Unsupported_Media_Type = 415,
    //     Too_Many_Requests = 429,
    //     Client_Closed_Request = 499,
    //     Internal_Server_Error = 500,
    //     Not_Implemented = 501,
    //     Bad_Gateway = 502,
    //     Service_Unavailable = 503,
    //     Gateway_Timeout = 504,
    //     Insufficient_Storage = 507

std::string_view http::get_status_msg(http::code http_code) {
    switch (http_code) {
    case http::code::OK: return "HTTP/1.1 200 OK";
    case http::code::Created: return "HTTP/1.1 201 Created";
    case http::code::Accepted: return "HTTP/1.1 202 Accepted";
    case http::code::No_Content: return "HTTP/1.1 204 No Content";
    case http::code::Moved_Permanently: return "HTTP/1.1 301 Moved Permanently";
    case http::code::Found: return "HTTP/1.1 302 Found";
    case http::code::See_Other: return "HTTP/1.1 303 See Other";
    case http::code::Not_Modified: return "HTTP/1.1 304 Not Modified";
    case http::code::Bad_Request: return "HTTP/1.1 400 Bad Request";
    case http::code::Unauthorized: return "HTTP/1.1 401 Unauthorized";
    case http::code::Forbidden: return "HTTP/1.1 403 Forbidden";
    case http::code::Not_Found: return "HTTP/1.1 404 Not Found";
    case http::code::Method_Not_Allowed: return "HTTP/1.1 405 Method Not Allowed";
    case http::code::Unsupported_Media_Type: return "HTTP/1.1 415 Unsupported Media Type";
    case http::code::Too_Many_Requests: return "HTTP/1.1 419 Too Many Requests";
    case http::code::Client_Closed_Request: return "HTTP/1.1 499 Client Closed Request";
    case http::code::Internal_Server_Error: return "HTTP/1.1 500 Internal Server Error";
    case http::code::Not_Implemented: return "HTTP/1.1 501 Not Implemented";
    case http::code::Bad_Gateway: return "HTTP/1.1 502 Bad Gateway";
    case http::code::Service_Unavailable: return "HTTP/1.1 503 Service Unavailable";
    case http::code::Gateway_Timeout: return "HTTP/1.1 504 Gateway Timeout";
    case http::code::Insufficient_Storage: return "HTTP/1.1 507 Insufficient Storage";
    default: return "HTTP/1.1 500 Internal Server Error"; 
    }
}

std::string_view http::extract_header_line(std::span<const char> buffer) {
    std::string_view response(buffer.data(), buffer.size());
    std::size_t end;
    if((end = response.find("\r\n")) == std::string::npos) {
        return "";
    }
    return response.substr(0, end);
}

std::string_view http::get_request_target(std::span<const char> buffer) {
    std::string_view req{ buffer.data(), buffer.size() };

    auto a = req.find(' ');
    if (a == std::string_view::npos) {
        throw HTTPException(code::Bad_Request,
            "malformed request: no spaces");
    }

    auto b = req.find(' ', a + 1);
    if (b == std::string_view::npos) {
        throw HTTPException(code::Bad_Request,
            "malformed request: no second space");
    }

    return req.substr(a + 1, b - a - 1);
}

std::string http::extract_endpoint(std::span<const char> buffer) {
    auto target = get_request_target(buffer);
    auto qpos = target.find('?');
    auto path = (qpos == std::string_view::npos
                 ? target
                 : target.substr(0, qpos));
    return std::string(path);
}

std::string_view http::extract_query_string(std::span<const char> buffer) {
    auto target = get_request_target(buffer);

    auto qpos = target.find('?');
    if (qpos == std::string_view::npos) {
        return {};
    }
    return target.substr(qpos + 1);
}

std::string_view http::extract_body(std::span<const char> buffer) {
    std::string_view header(buffer.data(), buffer.size());

    std::size_t start;
    if ((start = header.find("\r\n\r\n")) == std::string::npos && (start = header.find("\n\n")) == std::string::npos) {
        throw http::HTTPException(http::code::Bad_Request, "Failed to extract body from buffer");
    }
    std::size_t offset = (header[start] == '\r') ? 4 : 2;

    std::size_t end;
    if ((end = header.find("\r\n", start + offset)) == std::string::npos && (end = header.find("\n", start + offset)) == std::string::npos) {
        end = header.size();
    }

    return header.substr(start + offset, end - (start + offset));
}

http::code http::find_content_type(std::span<const char> buffer, std::string& content_type) noexcept {
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

http::code http::build_json(std::span<const char> buffer, http::json& json_array) {
    http::code code;
    std::string content_type;

    if((code = http::find_content_type(buffer, content_type)) != http::code::OK) {
        return code;
    }

    if(content_type == "application/x-www-form-urlencoded") {
        std::string body = (std::string)http::extract_body(buffer);
        json_array = http::parse_url_form(body);
    }
    else if (content_type == "application/json") {
        std::string_view body = http::extract_body(buffer);
        json_array = http::json::parse(body);
    }
    else {
        return http::code::Not_Implemented;
    }

    return http::code::OK;
}

 http::method http::extract_method(std::span<const char> buffer) {
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
    
}

std::unordered_map<std::string, std::string> http::extract_headers(std::span<const char> buffer) {
    std::unordered_map<std::string, std::string> headers;
    std::string_view request(buffer.data(), buffer.size());
    const std::string_view line_end = "\r\n";
    const std::string_view header_splitter = ": ";

    std::size_t headers_end = request.find("\r\n\r\n");
    if (headers_end == std::string_view::npos) {
        throw http::HTTPException(http::code::Bad_Request, "failed to extract headers from request buffer");
    }

    std::size_t pos = 0;
    std::size_t end_of_request_line = request.find(line_end, pos);
    if (end_of_request_line == std::string_view::npos) {
        throw http::HTTPException(http::code::Bad_Request, "failed to extract headers from request buffer");
    }
    
    pos = end_of_request_line + line_end.size();
    if (pos >= headers_end) {
        return headers;
    }

    while (true) {
        if (pos >= headers_end) {
            break;
        }
        std::size_t end = request.find(line_end, pos);
        if (end == std::string_view::npos) {
            throw http::HTTPException(http::code::Bad_Request, "failed to extract headers from request buffer");
        }

        std::string_view line = request.substr(pos, end - pos);
        if (line.empty()) {
            break;
        }

        std::size_t splitter_pos = line.find(header_splitter);
        if (splitter_pos == std::string_view::npos) {
            throw http::HTTPException(http::code::Bad_Request, "failed to extract headers from request buffer");
        }

        std::string key = std::string(line.substr(0, splitter_pos));
        std::string value = std::string(line.substr(splitter_pos + header_splitter.size()));

        headers[std::move(key)] = std::move(value);
        pos = end + line_end.size();
    }
    return headers;
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

http::code http::extract_status_code(std::span<const char> buffer) noexcept {
    std::string_view response(buffer.data(), buffer.size());
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

static bool extract_header_field(std::span<const char> buffer, std::string field, std::string& result) {
    field += ": ";
    std::string_view header(buffer.data(), buffer.size());
    std::size_t start, end;
    if((start = header.find(field)) == std::string::npos || (end = header.find("\r\n", start)) == std::string::npos || (start += field.size()) > end) {
        return false;
    }
    result = header.substr(start, end - start);
    return true;
}

http::code http::extract_token(const std::vector<char>& buffer, std::string& token) {
    std::string_view header(buffer.data(), buffer.size());
    std::string field;
    if(!extract_header_field(std::span<const char>(buffer), "Authorization", field)) {
        return http::code::Bad_Request;
    }

    std::string delim = "Bearer ";
    std::size_t start;
    if((start = field.find(delim)) == std::string::npos) {
        return http::code::Bad_Request;
    }

    token = field.substr(delim.length());
    return http::code::OK;
}

static bool is_valid_json(std::string_view body) {
    try {
        auto _ = nlohmann::json::parse(body.begin(), body.end());
    } catch (const nlohmann::json::parse_error&) {
        return false;
    }
    return true;
}

static bool is_valid_url_form(std::string_view sv) {
    if (sv.empty()) return false;
    size_t i = 0, n = sv.size();
    while (i < n) {
        auto eq = sv.find('=', i);
        if (eq == std::string_view::npos || eq == i)
            return false;
        for (size_t k = i; k < eq; ++k) {
            char c = sv[k];
            if (c == '%') {
                if (k+2 >= n
                 || !std::isxdigit(sv[k+1])
                 || !std::isxdigit(sv[k+2]))
                    return false;
                k += 2;
            }
            else if (c=='+' || std::isalnum(c)
                  || c=='-' || c=='_' 
                  || c=='.' || c=='~') {
            } else return false;
        }
        auto amp = sv.find('&', eq+1);
        auto end = (amp == std::string_view::npos ? n : amp);
        for (size_t v = eq+1; v < end; ++v) {
            char c = sv[v];
            if (c == '%') {
                if (v+2 >= n
                 || !std::isxdigit(sv[v+1])
                 || !std::isxdigit(sv[v+2]))
                    return false;
                v += 2;
            }
            else if (c=='+' || std::isalnum(c)
                  || c=='-' || c=='_'
                  || c=='.' || c=='~') {
            } else return false;
        }
        if (amp == std::string_view::npos)
            break;
        i = amp + 1;
    }
    return true;
}

static bool get_content_type(std::span<const char> buf, std::string& ct) {
    if (!extract_header_field(buf, "Content-Type", ct)) {
        return false;
    }
    std::transform(ct.begin(), ct.end(), ct.begin(), [](unsigned char c){ return std::tolower(c); });
    auto semi = ct.find(';');
    if (semi != std::string::npos) {
        ct.resize(semi);
    }
    if(ct.empty()) {
        return false;
    }
    return true;
}

static std::string_view get_args(std::span<const char> buffer, const std::string& desired, bool (*filter)(std::string_view) ) {
    std::string content_type;
    std::string_view body = http::extract_body(buffer);
    if (!get_content_type(buffer, content_type)) {
        throw http::HTTPException(http::code::Unsupported_Media_Type, std::format("expected={}, none provided", desired));
    }
    if (content_type != desired) {
        throw http::HTTPException(http::code::Unsupported_Media_Type, std::format("expected={}, client claimed={}", desired, content_type));
    }
    if (!filter(body)) {
        throw http::HTTPException(http::code::Bad_Request, std::format("invalid `{}` body", desired));
    }
    return body;
}

static std::string_view body_any(std::span<const char> buffer) {
    std::string_view body = http::extract_body(buffer);
    std::string content_type;
    if(!get_content_type(buffer, content_type)) {
        return body;
    } else if(content_type == "application/json" && is_valid_json(body)) {
        return body;
    } else if(content_type == "application/x-www-form-urlencoded" && is_valid_url_form(body)) {
        return body;
    } else if (content_type == "application/json" || content_type == "application/x-www-form-urlencoded") { // supported content type, but body was invalid
        throw http::HTTPException(http::code::Bad_Request, std::format("invalid argument format in request body, client claimed={}", content_type));
    } else { // cannot validate this content-type, simply pass through
        return body;
    }
}

static std::string_view args_any(std::span<const char> buffer) {
    std::string_view result;
    if(!(result = http::extract_query_string(buffer)).empty()) {
        return result; // prioritize query string
    }
    return body_any(buffer);
}

std::string_view http::extract_args(std::span<const char> buffer, http::arg_type arg) {
    switch(arg) {
        case http::arg_type::None: return {};
        case http::arg_type::Any: return args_any(buffer);
        case http::arg_type::Body_Any: return body_any(buffer);
        case http::arg_type::Body_JSON: return get_args(buffer, "application/json", is_valid_json);
        case http::arg_type::Body_URL: return get_args(buffer, "application/x-www-form-urlencoded", is_valid_url_form);
        case http::arg_type::Query_String: return http::extract_query_string(buffer);
        default:
            throw http::HTTPException(http::code::Internal_Server_Error, "unknown arg type"); // should'nt ever reach, unless enum class gets updated
    }
}

asio::awaitable<http::io::WriteStatus> http::io::co_write_all(Socket* sock, std::span<const char> buffer) noexcept {
    TransferState state;
    state.bytes_sent = 0;
    state.total_bytes = buffer.size();
    while(state.bytes_sent < state.total_bytes) {
        auto [ec, bytes_written] = co_await sock->co_write(buffer.data() + state.bytes_sent, state.total_bytes - state.bytes_sent);
        
        if(http::io::is_permanent_failure(ec) || state.retry_count > TransferState::MAX_RETRIES) {
            co_return http::io::WriteStatus{http::io::error_to_status(ec), 
            std::format("error={} ({})", ec.value(), ec.message()), static_cast<std::size_t>(state.bytes_sent)};
        }

        if(http::io::is_retryable(ec)) {
            co_await http::io::backoff(ec, state.retry_count);
            state.retry_count++;
        }

        state.bytes_sent += bytes_written;
    }
    if(state.bytes_sent != state.total_bytes) {
        co_return http::io::WriteStatus{http::code::Internal_Server_Error, 
                std::format("incomplete send, sent %zd/%zd bytes", state.bytes_sent, state.total_bytes), static_cast<std::size_t>(state.bytes_sent)};
    }
    co_return http::io::WriteStatus{http::code::OK, "Success", static_cast<std::size_t>(state.bytes_sent)};
}

std::chrono::milliseconds http::io::select_backoff(const asio::error_code& ec, int attempt) noexcept {
    if(!ec || http::io::is_client_disconnect(ec) || http::io::is_permanent_failure(ec)) {
        return std::chrono::milliseconds{0}; 
    }

    if(ec == asio::error::no_buffer_space) {
        return std::chrono::milliseconds{1000}*attempt;
    }

    constexpr int MAX_ATTEMPTS = 6;
    constexpr auto BASE_DELAY_MS = std::chrono::milliseconds{50};
    constexpr auto MAX_DELAY_MS = std::chrono::milliseconds{2000};
    static thread_local std::mt19937_64 rng(std::random_device{}());

    auto exp_delay = BASE_DELAY_MS * (1 << std::min(attempt, MAX_ATTEMPTS)); // multiply by 2^attempt
    if(exp_delay > MAX_DELAY_MS) {
        exp_delay = MAX_DELAY_MS;
    }
    auto jitter_range = exp_delay.count() / 10; // 10% of the ticks for the delay, to avoid contention on wake ups
    std::uniform_int_distribution<int64_t> dist(-jitter_range, jitter_range);
    return exp_delay + std::chrono::milliseconds{dist(rng)};
}

asio::awaitable<void> http::io::backoff(const asio::error_code& ec, int attempt) noexcept {
    auto delay = http::io::select_backoff(ec, attempt);
    if(delay.count() <= 0) {
        co_return;
    }

    asio::steady_timer timer(co_await asio::this_coro::executor);
    timer.expires_after(delay);
    co_await timer.async_wait(asio::use_awaitable);
    co_return;
}

