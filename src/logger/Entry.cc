#include "logger/Entry.h"
#include <cstdarg>
#include <execinfo.h>

namespace {
    int duration_ms(const core::WallTimePoint& start_time, const core::WallTimePoint& end_time) {
        return static_cast<int>(std::chrono::duration_cast<core::Ms>(end_time - start_time).count());
    }
}

namespace logger {

std::string get_time() {
    auto now = core::WallClock::now();
    std::time_t time = core::WallClock::to_time_t(now);
    std::tm local_time = *std::localtime(&time);

    std::ostringstream oss;
    oss << std::put_time(&local_time, "%Y-%m-%d %H:%M");
    return oss.str();
}

static std::string format_bytes(size_t bytes) {
    if(bytes == 1u) {
        return " byte";
    }

    static constexpr std::string_view units[] = {" bytes", " KB", " MB", " GB", " TB"};
    auto double_bytes = static_cast<double>(bytes);

    uint32_t i = 0u;
    for(; i < 5u && double_bytes > 1024; i++) {
        double_bytes /= 1024;
    }

    return std::to_string(static_cast<int>(double_bytes)) + std::string(units[i]);
}


static void trim_user_agent(std::string& user_agent) {
    std::string user_agent_label = "User-Agent: ";
    if (user_agent.find(user_agent_label) == 0)
    {
        user_agent = user_agent.substr(user_agent_label.length());
    }
}

static std::string get_identifier(const std::string& user_agent) {
    std::string result;
    const size_t identifier_end = user_agent.find("(");
    if (identifier_end != std::string::npos)
    {
        result = user_agent.substr(0, identifier_end - 1);
    }
    return result;
}

static std::string get_os(const std::string& user_agent) {
    std::string result;
    size_t os_start = user_agent.find("(");
    size_t os_end = user_agent.find(")", os_start);

    if (os_start != std::string::npos && os_end != std::string::npos)
    {
        std::string os_info = user_agent.substr(os_start + 1, os_end - os_start - 1);
        result += " on " + os_info;
    }
    return result;
}

static std::string get_browser(const std::string user_agent) {
    std::string result = "";
    size_t browser_start = std::string::npos;
    std::string browser_info;
    std::vector<std::string> browsers = {"Chrome/", "Firefox/", "Safari/", "Edge/", "Opera/", "Brave/", "Chromium/"};
    for (const auto& browser : browsers) {
        browser_start = user_agent.find(browser);
        if (browser_start != std::string::npos)
        {
            size_t browser_end = user_agent.find(" ", browser_start);
            browser_info = user_agent.substr(browser_start, browser_end - browser_start);
            break;
        }
    }
    if (!browser_info.empty()) {
        result += " " + browser_info;
    }
    return result;
}

std::string fmt_msg(const char* fmt, ...) {
    constexpr size_t BUFF_SIZE = 1024; // shoudnt ever need more than this
    char buf[BUFF_SIZE];
    va_list args;
    va_start(args, fmt);
    int size = std::vsnprintf(buf, BUFF_SIZE, fmt, args);
    va_end(args);
    if (size >= 0 && static_cast<size_t>(size) < BUFF_SIZE) {
        return std::string(buf, size);
    }
    return "";
}

std::string get_stack_trace() {
    const int max_frames = 64;
    void* addr_list[max_frames + 1];

    int addr_len = backtrace(addr_list, sizeof(addr_list) / sizeof(void*));
    if(!addr_len) {
        return "<empty, possible corruption>\n";
    }

    char** symbol_list = backtrace_symbols(addr_list, addr_len);
    std::ostringstream oss;
    for(int i = 0; i < addr_len; ++i) {
        oss << symbol_list[i] << "\n";
    }
    free(symbol_list);
    return oss.str();
}

std::string get_user_agent(std::span<const char> buffer) {
    size_t ua_start, ua_end;
    std::string_view buffer_str(buffer.data(), buffer.size());
    if((ua_start = buffer_str.find("User-Agent")) == std::string::npos)
        return "";
    if((ua_end = buffer_str.find("\r\n", ua_start)) == std::string::npos)
        return "";

    std::string user_agent(buffer_str.substr(ua_start, ua_end - ua_start));
    trim_user_agent(user_agent);
    return get_identifier(user_agent) + get_os(user_agent) + get_browser(user_agent);
}

std::string get_header_line(std::span<const char> buffer) {
    size_t end;
    std::string_view header(buffer.data(), buffer.size());
    if((end = header.find("\r\n")) == std::string::npos)
        return "";
    return std::string(header.substr(0, end));
}

std::string level_to_str(Level level) {
    switch(level) {
        case Level::Trace: return "TRACE";
        case Level::Debug: return "DEBUG";
        case Level::Info: return "INFO";
        case Level::Warn: return "WARN";
        case Level::Error: return "ERROR";
        case Level::Fatal: return "FATAL";
        case Level::Status: return "STATUS";
        default: return "";
    }
}

std::string InlineEntry::Build() {
    std::string level_str = level_to_str(level) + " ";
    std::string curr_time = "[" + get_time() +"] ";
    context = "[" + context + "] ";
    std::string res = curr_time + level_str + context + message;
    res += (level != logger::Level::Status && level != logger::Level::Info) ?
            " (" + std::string(file) + ":" + std::to_string(line) + ", " + std::string(function) + ")" : "";
    return res + "\n";
}

std::string SessionEntry::Build() {
    const std::string time = "[" + get_time() + "] ";
    const std::string client = " [client " + RemoteIpPortString + "] ";
    const std::string request_str = "\"" + RequestLine + "\" ";
    std::string latency_RTT_size = " [RTT: "  + std::to_string(duration_ms(RttStart, RttEnd)) + " ms";
    if(BytesServed > 0) {
        latency_RTT_size += " Size: " + format_bytes(BytesServed);
    }
    latency_RTT_size += "] ";

    return time + level_to_str(level) + client + request_str + UserAgent + latency_RTT_size + ResponseLine + "\n";
}

}
