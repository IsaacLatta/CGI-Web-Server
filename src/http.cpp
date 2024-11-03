#include "http.h" 

std::string get_response(const std::string& response_header)
{
    std::size_t pos = response_header.find("\r\n");
    if(pos == std::string::npos)
        return "";
    return response_header.substr(0, pos);
}

int duration_ms(std::chrono::time_point<std::chrono::system_clock>& start_time, std::chrono::time_point<std::chrono::system_clock>& end_time)
{
    return static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count());
}

std::string formatBytes(long bytes)
{
    if(bytes == 1) return " byte";

    std::string units[] = {" bytes", " KB", " MB", " GB", " TB"};
    int i = 0;
    double double_bytes = static_cast<double>(bytes);
    for(i = 0; i < 5 && double_bytes > 1024; i++)
    {
        double_bytes /= 1024;
    }
    return std::to_string(static_cast<int>(double_bytes)) + units[i];
}

std::string getTime()
{
    auto now = std::chrono::system_clock::now();
    std::time_t time = std::chrono::system_clock::to_time_t(now);
    std::tm local_time = *std::localtime(&time);

    std::ostringstream oss;
    oss << std::put_time(&local_time, "%Y-%m-%d %H:%M");
    return oss.str();
}

std::string getDate(std::string fmt_time)
{
    return fmt_time.substr(0, fmt_time.find(" "));
}

void trim_user_agent(std::string& user_agent)
{
    std::string user_agent_label = "User-Agent: ";
    if (user_agent.find(user_agent_label) == 0) 
    {
        user_agent = user_agent.substr(user_agent_label.length());
    }
}

std::string get_identifier(const std::string& user_agent)
{
    std::string result;
    std::size_t identifier_end = user_agent.find("(");
    if (identifier_end != std::string::npos) 
    {
        result = user_agent.substr(0, identifier_end - 1); 
    }
    return result;
}

std::string get_os(const std::string& user_agent)
{
    std::string result;
    std::size_t os_start = user_agent.find("(");
    std::size_t os_end = user_agent.find(")", os_start);

    if (os_start != std::string::npos && os_end != std::string::npos) 
    {
        std::string os_info = user_agent.substr(os_start + 1, os_end - os_start - 1);
        result += " on " + os_info;
    }
    return result;
}

std::string get_browser(const std::string user_agent)
{   
    std::string result = "";
    std::size_t browser_start = std::string::npos;
    std::string browser_info;
    std::vector<std::string> browsers = {"Chrome/", "Firefox/", "Safari/", "Edge/", "Opera/", "Brave/", "Chromium/"};
    for (const auto& browser : browsers) 
    {
        browser_start = user_agent.find(browser);
        if (browser_start != std::string::npos) 
        {
            std::size_t browser_end = user_agent.find(" ", browser_start);
            browser_info = user_agent.substr(browser_start, browser_end - browser_start);
            break; 
        }
    }
    if (!browser_info.empty()) 
    {
        result += " " + browser_info;
    }
    return result;
}

std::string get_user_agent(const std::vector<char>& buffer)
{
    int ua_start, ua_end;
    std::string buffer_str(buffer.begin(), buffer.end());
    if((ua_start = buffer_str.find("User-Agent")) == std::string::npos)
        return "";
    if((ua_end = buffer_str.find("\r\n", ua_start)) ==std::string::npos)
        return "";

    std::string user_agent =  buffer_str.substr(ua_start, ua_end - ua_start);
    trim_user_agent(user_agent);
    return get_identifier(user_agent) + get_os(user_agent) + get_browser(user_agent);
}

std::string get_header(const std::vector<char>& buffer)
{
    int end;
    std::string header_buffer(buffer.begin(), buffer.end());
    if((end = header_buffer.find("\r\n")) == std::string::npos || end < 0)
        return "";
    return header_buffer.substr(0,end);
}

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
        {".js", "application/javascript"}, {".png", "image/png"}, {".jpg", "image/jpeg"}, {".ico", "image/ico"},
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
    resource = request.substr(start + 1, end - start - 1);
    if(resource == "/")
    {
        resource = "/index.html";
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