#include "logger.h"
#include "config.h"

using namespace logger;

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

static std::string get_time() {
    auto now = std::chrono::system_clock::now();
    std::time_t time = std::chrono::system_clock::to_time_t(now);
    std::tm local_time = *std::localtime(&time);

    std::ostringstream oss;
    oss << std::put_time(&local_time, "%Y-%m-%d %H:%M");
    return oss.str();
}

static std::string get_date() {
    std::string fmt_time = get_time();
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

std::string logger::fmt_msg(const char* fmt, ...) {
    constexpr size_t BUFF_SIZE = 256; // shoudnt ever need more than this
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

std::string create_log(const logger::SessionEntry& info, std::string_view type) {
    std::string time = "[" + get_time() + "] ";
    std::string client = " [client " + info.client_addr + "] "; 
    std::string request = "\"" + info.request + "\" ";
    std::string latency_RTT_size = " [Latency: " + std::to_string(duration_ms(info.Latency_start_time, info.Latency_end_time)) 
                                 + " ms RTT: "  + std::to_string(duration_ms(info.RTT_start_time, info.RTT_end_time)) + " ms";
    if(info.bytes != 0) {
        latency_RTT_size += " Size: " + format_bytes(info.bytes); 
    }
    latency_RTT_size += "] ";
    
    return time + std::string(type) + client + request + info.user_agent + latency_RTT_size + info.response + "\n";
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

static std::string level_to_str(logger::level level) {
    switch(level) {
        case level::Trace: return "TRACE";
        case level::Debug: return "DEBUG";
        case level::Info: return "INFO";
        case level::Warn: return "WARN";
        case level::Error: return "ERROR";
        case level::Fatal: return "FATAL";
        case level::Status: return "STATUS";
        default: return "";
    }
}

// static const std::string& server_name = cfg::Config::getInstance()->getServerName();
// static const std::string& config_log_path = cfg::Config::getInstance()->getLogPath();

// std::string get_file_name() {
//     std::string log_dir = "log";
//         if(!config_log_path.empty()) {
//         char resolved[PATH_MAX];
//         if(realpath(config_log_path.c_str(), resolved)) {
//             log_dir = resolved;
//         }
//     }
//     std::filesystem::create_directories(log_dir);
//     std::string res_path = log_dir + "/" + server_name + "-" + get_date(get_time()) + ".log";
//     return res_path;
// }

logger::FileSink::FileSink(const std::string& path, const std::string& server_name) 
    : path(path), server_name(server_name), fd(-1), date_today(get_date()) 
{
    openFile();
}

logger::FileSink::~FileSink() {
    if(fd != -1) {
        ::close(fd);
    }
}

void logger::FileSink::rotateFile() {
    if(fd != -1) {
        ::close(fd);
    }
    openFile();
}

void logger::FileSink::openFile() {
    file_name = path + "/" + server_name + "-" + get_date() + ".log";
    if ((fd = fd = ::open(file_name.c_str(), O_WRONLY | O_CREAT | O_APPEND, 0644))  < 0) {
        perror("openFile failed");
    }
    date_today = get_date();
}

void logger::FileSink::write(const std::string& log_msg) {
    if(date_today != get_date()) {
        rotateFile();
    }
    
    std::size_t bytes_written = 0, msg_size = log_msg.length();
    while(bytes_written < msg_size) {
        ssize_t bytes = ::write(fd, log_msg.c_str() + bytes_written, msg_size - bytes_written);
        if(bytes < 0) {
            perror("write failed");
            break; 
        }
        bytes_written += bytes;
    }
}

std::string InlineEntry::build() {
    std::string level_str = level_to_str(level) + " ";
    std::string curr_time = "[" + get_time() +"] ";
    context = "[" + context + "] ";
    return curr_time + level_str + context + message + 
            " (" + std::string(file) + ":" + std::to_string(line) + ", " + std::string(function) + ")\n";
}

std::string SessionEntry::build() {
    std::string time = "[" + get_time() + "] ";
    std::string client = " [client " + client_addr + "] "; 
    std::string request_str = "\"" + request + "\" ";
    std::string latency_RTT_size = " [Latency: " + std::to_string(duration_ms(Latency_start_time, Latency_end_time)) 
                                 + " ms RTT: "  + std::to_string(duration_ms(RTT_start_time, RTT_end_time)) + " ms";
    if(bytes != 0) {
        latency_RTT_size += " Size: " + format_bytes(bytes); 
    }
    latency_RTT_size += "] ";
    
    return time + level_to_str(level) + client + request_str + user_agent + latency_RTT_size + response + "\n";
}

Logger Logger::INSTANCE;

logger::Logger* logger::Logger::getInstance() {
    return &Logger::INSTANCE;
}

void Logger::addSink(std::unique_ptr<Sink>&& sink) {
    if(sink_count >= logger::MAX_SINKS) {
        return;
    }
    
    sinks[sink_count] = std::move(sink);
    sink_count++;
}

void Logger::push(std::unique_ptr<logger::Entry>&& entry) {
    std::size_t pos = head.fetch_add(1, std::memory_order_release);
    std::size_t index = pos % logger::LOG_BUFFER_SIZE; 
    log_buffer[index] = std::move(entry);
}

std::unique_ptr<logger::Entry> Logger::pop() {
    std::size_t curr_tail = tail.load(std::memory_order_relaxed);
    std::size_t curr_head = head.load(std::memory_order_acquire);

    std::size_t entries_not_flushed = curr_head - curr_tail;
    if(entries_not_flushed >= logger::LOG_BUFFER_SIZE) { // buffer overun, drop old entries
        tail.store(curr_head - logger::LOG_BUFFER_SIZE + 1, std::memory_order_relaxed);
        curr_tail = tail.load(std::memory_order_relaxed);
    }

    if(curr_tail >= curr_head) {
        return nullptr; // no logs to write
    }
    tail.fetch_add(1, std::memory_order_release);
    return std::move(log_buffer[curr_tail % logger::LOG_BUFFER_SIZE]);
}

void Logger::flush() {
    std::string logs = "";
    for(int i = 0; i < entries; ++i) {
        logs += batch[i]->build();
    }   
    for(int i = 0; i < sink_count; ++i) {
        if(sinks[i] != nullptr) {
            sinks[i]->write(logs);
        }
    }
    entries = 0;
}

void Logger::processBatch() {
    bool timeout = (std::chrono::steady_clock::now() - last_flush) >= logger::BATCH_TIMEOUT;
    if (entries > 0 && timeout) {
        flush();
        last_flush = std::chrono::steady_clock::now();
    }
    
    auto entry = pop();
    if(entry == nullptr) {
        return;
    }

    batch[entries] = std::move(entry);
    entries++;
    if (entries >= logger::ENTRY_BATCH_SIZE) {
        flush();
        last_flush = std::chrono::steady_clock::now();
    }
}

void Logger::run() {
    while (running.load(std::memory_order_acquire)) {
        processBatch();
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    if(entries > 0) {
        flush();
    }
}

void Logger::start() {
    this->running = true;
    this->worker_handle = std::thread(&logger::Logger::run, this);
}

Logger::~Logger() {
    running.store(false, std::memory_order_release);
    if(worker_handle.joinable()) {
        worker_handle.join();
    }
}

void ConsoleSink::write(const std::string& log_msg) {
    std::size_t msg_len = log_msg.length(), bytes_written = 0;
    while(bytes_written < msg_len) {
        ssize_t bytes = ::write(STDOUT_FILENO, log_msg.c_str() + bytes_written, msg_len - bytes_written);
        if(bytes < 0) {
            perror("write error: console sink");
            break;
        }
        bytes_written += bytes;
    }
}




