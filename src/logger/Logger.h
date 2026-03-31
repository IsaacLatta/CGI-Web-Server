#pragma once

#include <string>
#include <thread>
#include <atomic>
#include <memory>
#include <array>

#include "logger/forward.h"

namespace logger  {

class Logger {
public:
    static Logger& GetInstance() {
        static Logger s_logger;
        return s_logger;
    }

public:
    void AddSink(std::unique_ptr<Sink>&& sink);
    void Push(std::unique_ptr<Entry>&& entry);
    void Start();
    void StopAndFlush();

    Logger(const Logger&) = delete;
    Logger& operator=(Logger&) = delete;
    Logger& operator=(Logger&&) = delete;
    Logger(Logger&&) = delete;

private:
    Logger() = default;
    ~Logger();

    void Run(std::stop_token);
    void ProcessBatch();
    std::unique_ptr<Entry> Pop();
    void Flush();

private:
    std::array<std::unique_ptr<Sink>, MAX_SINKS> sinks_{};
    size_t sink_count_ { 0u };

    std::array<std::unique_ptr<Entry>, LOG_BUFFER_SIZE> log_buffer_{};
    std::atomic<size_t> head_{ 0u };
    std::atomic<size_t> tail_{ 0u };

    size_t entries_ { 0u };
    core::MonoTimePoint last_flush_ { core::MonoClock::now() };
    std::array<std::unique_ptr<Entry>, ENTRY_BATCH_SIZE> batch_{};

    std::atomic<bool> running_ { false };
    std::jthread worker_handle_{};
    std::atomic<Level> log_threshold_ { Level::Trace };
};

}