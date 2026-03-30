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
#include <thread>
#include <atomic>
#include <memory>
#include <array>
#include <execinfo.h>

#include "Sink.h"

#include "core/time.h"

namespace logger 
{ 
    std::string get_time();
    std::string fmt_msg(const char* fmt, ...);
    std::string get_stack_trace();
    std::string get_user_agent(std::span<const char>);
    std::string get_header_line(std::span<const char>);

    constexpr std::size_t MAX_SINKS = 1024;
    constexpr std::size_t LOG_BUFFER_SIZE = 1024;
    constexpr std::size_t ENTRY_BATCH_SIZE = 10;
    constexpr auto BATCH_TIMEOUT = std::chrono::milliseconds(10);

    enum class level {
        Trace, Debug, Info, Warn, Error, Fatal, Status 
    };

    struct Entry {
        virtual ~Entry() = default;

        int line;
        const char* file;
        const char* function;
        logger::level level;
        std::string message;
        virtual std::string build() = 0;
    };

    struct InlineEntry: public Entry {
        std::string context;
        std::string build() override;
    };

    struct SessionEntry : public Entry {
        size_t BytesServed { 0u };
        std::string UserAgent;
        std::string RequestLine;
        std::string ResponseLine;
        std::string RemoteIpPortString;
        core::WallTimePoint RttStart { core::WallClock::now() };
        core::WallTimePoint RttEnd;

        std::string build() override;
    };

    class Logger {
    public:
        static Logger* getInstance();

    public:
        void addSink(std::unique_ptr<Sink>&& sink);
        void push(std::unique_ptr<logger::Entry>&& entry);
        void start();
        void stopAndFlush();

        private:
        static Logger INSTANCE;

        std::array<std::unique_ptr<Sink>, MAX_SINKS> sinks;
        std::size_t sink_count{0};

        std::array<std::unique_ptr<logger::Entry>, logger::LOG_BUFFER_SIZE> log_buffer;
        std::atomic<std::size_t> head{0};
        std::atomic<std::size_t> tail{0};
    
        std::size_t entries{0};
        std::chrono::time_point<std::chrono::steady_clock> last_flush{std::chrono::steady_clock::now()};
        std::array<std::unique_ptr<logger::Entry>, ENTRY_BATCH_SIZE> batch;

        std::atomic<bool> running;
        std::thread worker_handle;
        std::atomic<logger::level> log_threshold;

        private: 
        ~Logger();
        Logger() {};
        Logger(const Logger&) = delete;
        void operator=(Logger&) = delete;

        void run();
        void processBatch();
        std::unique_ptr<logger::Entry> pop();
        void flush(); 
    };

};

#endif