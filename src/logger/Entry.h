#pragma once

#include <string>

#include "logger/forward.h"

namespace logger {
    std::string get_time();
    std::string fmt_msg(const char* fmt, ...);
    std::string get_stack_trace();
    std::string get_user_agent(std::span<const char>);
    std::string get_header_line(std::span<const char>);
    std::string level_to_str(Level level);

    struct Entry {
        virtual ~Entry() = default;

        int line { 0 };
        const char* file { nullptr };
        const char* function { nullptr };
        Level level { Level::Trace };
        std::string message;
        virtual std::string Build() = 0;
    };

    struct InlineEntry: public Entry {
        std::string context;
        std::string Build() override;
    };

    struct SessionEntry : public Entry {
        size_t BytesServed { 0u };
        std::string UserAgent;
        std::string RequestLine;
        std::string ResponseLine;
        std::string RemoteIpPortString;
        core::WallTimePoint RttStart { core::WallClock::now() };
        core::WallTimePoint RttEnd;

        std::string Build() override;
    };

}