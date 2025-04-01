#ifndef LOGGER_MACROS
#define LOGGER_MACROS

#include "logger.h"

#ifndef CURRENT_LOG_LEVEL
#define CURRENT_LOG_LEVEL logger::level::Trace
#endif

#define _LOG_INLINE(lvl, ctx, fmt, ...)                                  \
    do {                                                                 \
        if (static_cast<int>(lvl) >= static_cast<int>(CURRENT_LOG_LEVEL)) {\
            auto entry = std::make_unique<logger::InlineEntry>();         \
            entry->level = lvl;                                            \
            entry->message = logger::fmt_msg(fmt, ##__VA_ARGS__);          \
            entry->context = ctx; \
            if(lvl != logger::level::Status && lvl != logger::level::Info) { \
                entry->function = __func__;  \
                entry->line = __LINE__;  \
                entry->file = __FILE__; \
            }  \
            logger::Logger::getInstance()->push(std::move(entry));         \
        }                                                                \
    } while (0)

/**
 * @brief Logs a TRACE-level message.
 *
 * @param ctx   the context of the log
 * @param fmt   the printf-style format string.
 * @param ...   additional arguments for the format string
 * @note includes the line, function, and file at the end of the message
 */
#define TRACE(ctx, fmt, ...)  _LOG_INLINE(logger::level::Trace, ctx, fmt, ##__VA_ARGS__)

/**
 * @brief Logs a DEBUG-level message.
 *
 * @param ctx   the context of the log
 * @param fmt   the printf-style format string.
 * @param ...   additional arguments for the format string
 * @note includes the line, function, and file at the end of the message
 */
#define DEBUG(ctx, fmt, ...)  _LOG_INLINE(logger::level::Debug, ctx, fmt, ##__VA_ARGS__)

/**
 * @brief Logs a INFO-level message.
 *
 * @param ctx   the context of the log
 * @param fmt   the printf-style format string.
 * @param ...   additional arguments for the format string
 */
#define INFO(ctx, fmt, ...)   _LOG_INLINE(logger::level::Info,  ctx, fmt, ##__VA_ARGS__)

/**
 * @brief Logs a WARN-level message.
 *
 * @param ctx   the context of the log
 * @param fmt   the printf-style format string.
 * @param ...   additional arguments for the format string
 * @note includes the line, function, and file at the end of the message
 */
#define WARN(ctx, fmt, ...)   _LOG_INLINE(logger::level::Warn,  ctx, fmt, ##__VA_ARGS__)

/**
 * @brief Logs a ERROR-level message.
 *
 * @param ctx   the context of the log
 * @param fmt   the printf-style format string.
 * @param ...   additional arguments for the format string
 * @note includes the line, function, and file at the end of the message
 */
#define ERROR(ctx, fmt, ...)  _LOG_INLINE(logger::level::Error, ctx, fmt, ##__VA_ARGS__)

/**
 * @brief Logs a STATUS-level message.
 *
 * @param ctx   the context of the log
 * @param fmt   the printf-style format string.
 * @param ...   additional arguments for the format string
 */
#define STATUS(ctx, fmt, ...)  _LOG_INLINE(logger::level::Status, ctx, fmt, ##__VA_ARGS__)

/**
 * @brief Logs a FATAL-level message.
 *
 * @param ctx   the context of the log
 * @param fmt   the printf-style format string.
 * @param ...   additional arguments for the format string
 * @note appends the pid and stack trace to the end of the log, will flush the logger and exit the program
 */
#define FATAL(ctx, fmt, ...) \
    do { \
        _LOG_INLINE(logger::level::Fatal, ctx, \
            fmt ", exiting pid=%d\nSTACK TRACE:\n%s", ##__VA_ARGS__, \
            getpid(), logger::get_stack_trace().c_str()); \
        logger::Logger::getInstance()->stopAndFlush(); \
        exit(EXIT_FAILURE); \
    } while(0)

/**
 * @brief Logs a Session.
 *
 * @param entry a SessionEntry object (or rvalue) to be logged.
 * @note will take ownership of the session entry
 */
#define LOG_SESSION(entry) \
    do { \
        auto entry_ptr = std::make_unique<logger::SessionEntry>(entry); \
        logger::Logger::getInstance()->push(std::move(entry_ptr)); \
    } while(0) 

#endif