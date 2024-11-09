#include "logger.h"

std::string get_response(const std::string& response_header) {
    std::size_t pos = response_header.find("\r\n");
    if(pos == std::string::npos)
        return "";
    return response_header.substr(0, pos);
}

int duration_ms(const std::chrono::time_point<std::chrono::system_clock>& start_time, const std::chrono::time_point<std::chrono::system_clock>& end_time) {
    return static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count());
}

std::string format_bytes(long bytes) {
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

std::string get_time() {
    auto now = std::chrono::system_clock::now();
    std::time_t time = std::chrono::system_clock::to_time_t(now);
    std::tm local_time = *std::localtime(&time);

    std::ostringstream oss;
    oss << std::put_time(&local_time, "%Y-%m-%d %H:%M");
    return oss.str();
}

std::string get_date(std::string fmt_time) {
    return fmt_time.substr(0, fmt_time.find(" "));
}

void trim_user_agent(std::string& user_agent) {
    std::string user_agent_label = "User-Agent: ";
    if (user_agent.find(user_agent_label) == 0) 
    {
        user_agent = user_agent.substr(user_agent_label.length());
    }
}

std::string get_identifier(const std::string& user_agent) {
    std::string result;
    std::size_t identifier_end = user_agent.find("(");
    if (identifier_end != std::string::npos) 
    {
        result = user_agent.substr(0, identifier_end - 1); 
    }
    return result;
}

std::string get_os(const std::string& user_agent) {
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

std::string get_browser(const std::string user_agent) {   
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

std::string create_log(const logger::entry& info, std::string type) {
    std::string time = "[" + get_time() + "] ";
    std::string client = " [client " + info.client_addr + "] "; 
    std::string request = "\"" + info.request + "\" ";
    std::string latency_RTT_size = " [Latency: " + std::to_string(duration_ms(info.start_time, info.end_time)) 
                                 + " ms RTT: "  + std::to_string(duration_ms(info.RTT_start_time, info.end_time)) + " ms";
    if(info.bytes != 0) {
        latency_RTT_size += " Size: " + format_bytes(info.bytes); 
    }
    latency_RTT_size += "] ";
    
    return time + type + client + request + info.user_agent + latency_RTT_size + info.response + "\n";
}

std::string logger::get_user_agent(const char* buffer, std::size_t size) {
    std::size_t ua_start, ua_end;
    std::string_view buffer_str(buffer, size);
    if((ua_start = buffer_str.find("User-Agent")) == std::string::npos)
        return "";
    if((ua_end = buffer_str.find("\r\n", ua_start)) == std::string::npos)
        return "";

    std::string user_agent(buffer_str.substr(ua_start, ua_end - ua_start));
    trim_user_agent(user_agent);
    return get_identifier(user_agent) + get_os(user_agent) + get_browser(user_agent);
}

std::string logger::get_header_line(const char* buffer, std::size_t size) {
    std::size_t end;
    std::string_view header(buffer, size);
    if((end = header.find("\r\n")) == std::string::npos)
        return "";
    return std::string(header.substr(0, end));
}

void logger::log_file(const logger::entry& info, std::string&& type) {
    std::fstream log_file;
    std::string file_name = "web-" + get_date(get_time()) + ".log";
    log_file.open(file_name, std::fstream::app);
    if(!log_file.is_open()) {
        return;
    }
    std::string log_msg = create_log(info, type);
    LOG("INFO", "LOG MESSAGE", "\n%s", log_msg.c_str());
    log_file << log_msg;
}
