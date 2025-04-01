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
            entry->context = ctx;                                          \
            entry->function = __func__;                                    \
            entry->line = __LINE__;                                        \
            entry->file = __FILE__;                                        \
            logger::Logger::getInstance()->push(std::move(entry));         \
        }                                                                \
    } while (0)

#define TRACE(ctx, fmt, ...)  _LOG_INLINE(logger::level::Trace, ctx, fmt, ##__VA_ARGS__)
#define DEBUG(ctx, fmt, ...)  _LOG_INLINE(logger::level::Debug, ctx, fmt, ##__VA_ARGS__)
#define INFO(ctx, fmt, ...)   _LOG_INLINE(logger::level::Info,  ctx, fmt, ##__VA_ARGS__)
#define WARN(ctx, fmt, ...)   _LOG_INLINE(logger::level::Warn,  ctx, fmt, ##__VA_ARGS__)
#define ERROR(ctx, fmt, ...)  _LOG_INLINE(logger::level::Error, ctx, fmt, ##__VA_ARGS__)
#define FATAL(ctx, fmt, ...)  _LOG_INLINE(logger::level::Fatal, ctx, fmt, ##__VA_ARGS__)

#define LOG_SESSION(entry) \
    do { \
        auto entry_ptr = std::make_unique<logger::SessionEntry>(entry); \
        logger::Logger::getInstance()->push(std::move(entry_ptr)); \
    } while(0) 

#endif