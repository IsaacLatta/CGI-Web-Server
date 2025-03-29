#ifndef LOGGER_H
#define LOGGER_H

#include <chrono>
#include <cstring>
#include <ctime>
#include <string>
#include <iostream>
#include <ctime>
#include <cstdio>
#include <vector>
#include <fstream>
#include <format>
#include <unistd.h>
#include <limits.h>
#include <filesystem>

#define LOG(type, tag, format, ...) do { \
    time_t now = time(0); \
    struct tm* timeinfo = localtime(&now); \
    char timeStr[25]; \
    strftime(timeStr, sizeof(timeStr), "%a %b %d %H:%M:%S %Y", timeinfo); \
    const char* filename = strrchr(__FILE__, '/'); \
    filename = filename ? filename + 1 : __FILE__; \
    printf("[%s %s:%d] %s %s: " format "\n", \
        timeStr, filename, __LINE__, type, tag, ##__VA_ARGS__); \
} while(0)

#define DEBUG(tag, format, ...) do { \
    LOG("DEBUG", tag, format, ##__VA_ARGS__); \
} while(0)

#define LOG_ERROR(level, tag, error_code, error_msg, format, ...) do { \
    time_t now = time(0); \
    struct tm* timeinfo = localtime(&now); \
    char timeStr[25]; \
    strftime(timeStr, sizeof(timeStr), "%a %b %d %H:%M:%S %Y", timeinfo); \
    const char* filename = strrchr(__FILE__, '/'); \
    filename = filename ? filename + 1 : __FILE__; \
    printf("[%s %s:%d] %s %s: (Error Code: %d => %s) " format "\n", \
        timeStr, filename, __LINE__, level, tag, error_code, error_msg, ##__VA_ARGS__); \
} while(0)

#define ERROR(tag, error_code, error_msg, format, ...) do { \
    LOG_ERROR("ERROR", tag, error_code, error_msg, format, ##__VA_ARGS__); \
} while(0)


namespace logger 
{ 
    constexpr std::string_view ERROR = "ERROR";
    constexpr std::string_view WARN = "WARNING";
    constexpr std::string_view INFO = "INFO";
    constexpr std::string_view STATUS = "STATUS";
    constexpr std::string_view FATAL = "FATAL";

    struct Entry {
        unsigned long bytes{0};
        std::string user_agent{""}; 
        std::string request{""};
        std::string response{""};
        std::string client_addr{""};
        std::chrono::time_point<std::chrono::system_clock> Latency_start_time;
        std::chrono::time_point<std::chrono::system_clock> Latency_end_time;
        std::chrono::time_point<std::chrono::system_clock> RTT_start_time;
        std::chrono::time_point<std::chrono::system_clock> RTT_end_time;
    };

    std::string get_user_agent(const char* buffer, std::size_t size);
    std::string get_header_line(const char* buffer, std::size_t size);
    void log_session(const Entry& info, std::string_view level);
    void log_message(std::string_view level, std::string&& context, std::string&& msg);
};

#define EXIT_FATAL(tag, error_code, error_msg, format, ...) do { \
    time_t now = time(0); \
    struct tm* timeinfo = localtime(&now); \
    char timeStr[25]; \
    strftime(timeStr, sizeof(timeStr), "%a %b %d %H:%M:%S %Y", timeinfo); \
    const char* filename = strrchr(__FILE__, '/'); \
    filename = filename ? filename + 1 : __FILE__; \
    fprintf(stderr, "[%s %s:%d] FATAL %s: (%s: %d) " format ", exiting pid=%d\n", \
        timeStr, filename, __LINE__, tag, error_msg, error_code, ##__VA_ARGS__, getpid()); \
    exit(EXIT_FAILURE); \
} while (0)


#endif