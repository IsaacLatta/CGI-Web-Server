#ifndef LOGGER_H
#define LOGGER_H

#include <cstring>
#include <ctime>
#include <string>
#include <iostream>
#include <ctime>
#include <cstdio>

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
    printf("[%s %s:%d] %s %s: (Error Code: %d, Message: %s) " format "\n", \
        timeStr, filename, __LINE__, level, tag, error_code, error_msg, ##__VA_ARGS__); \
} while(0)

#define ERROR(tag, error_code, error_msg, format, ...) do { \
    LOG_ERROR("ERROR", tag, error_code, error_msg, format, ##__VA_ARGS__); \
} while(0)


#define EXIT_FATAL(tag, error_code, error_msg, format, ...) do { \
    time_t now = time(0); \
    struct tm* timeinfo = localtime(&now); \
    char timeStr[25]; \
    strftime(timeStr, sizeof(timeStr), "%a %b %d %H:%M:%S %Y", timeinfo); \
    const char* filename = strrchr(__FILE__, '/'); \
    filename = filename ? filename + 1 : __FILE__; \
    fprintf(stderr, "[%s %s:%d] FATAL %s: (%s: %d) " format " exiting pid=%d ...\n", \
        timeStr, filename, __LINE__, tag, error_msg, error_code, ##__VA_ARGS__, getpid()); \
    exit(EXIT_FAILURE); \
} while (0)



#endif