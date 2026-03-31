#include "logger/Logger.h"
#include "logger/Entry.h"
#include "logger/Sink.h"

namespace logger {
    void Logger::AddSink(std::unique_ptr<Sink>&& sink) {
        if(sink_count_ >= MAX_SINKS) {
            return;
        }

        sinks_[sink_count_] = std::move(sink);
        sink_count_++;
    }

    void Logger::Push(std::unique_ptr<Entry>&& entry) {
        const size_t pos = head_.fetch_add(1, std::memory_order_release);
        const size_t index = pos % LOG_BUFFER_SIZE;
        log_buffer_[index] = std::move(entry);
    }

    std::unique_ptr<Entry> Logger::Pop() {
        size_t curr_tail = tail_.load(std::memory_order_relaxed);
        const size_t curr_head = head_.load(std::memory_order_acquire);

        const size_t entries_not_flushed = curr_head - curr_tail;
        if(entries_not_flushed >= logger::LOG_BUFFER_SIZE) { // buffer overrun, drop old entries
            tail_.store(curr_head - logger::LOG_BUFFER_SIZE + 1, std::memory_order_relaxed);
            curr_tail = tail_.load(std::memory_order_relaxed);
        }

        if(curr_tail >= curr_head) {
            return nullptr; // no logs to write
        }

        tail_.fetch_add(1, std::memory_order_release);
        return std::move(log_buffer_[curr_tail % logger::LOG_BUFFER_SIZE]);
    }

    void Logger::StopAndFlush() {
        running_.store(false, std::memory_order_release);
        if(worker_handle_.joinable()) {
            worker_handle_.join();
        }

        if(entries_ > 0) { // flush the batch first
            Flush();
        }

        std::unique_ptr<logger::Entry> entry;
        while((entry = Pop()) != nullptr) {
            std::string log = entry->Build();
            for(int i = 0; i < sink_count_; ++i) { // flush remaining one by one
                sinks_[i]->Write(log);
            }
        }
    }

    void Logger::Flush() {
        std::string logs;
        for(auto i = 0; i < entries_; ++i) {
            logs += batch_[i]->Build();
        }
        for(auto i = 0; i < sink_count_; ++i) {
            if(sinks_[i] != nullptr) {
                sinks_[i]->Write(logs);
            }
        }
        entries_ = 0;
    }

    void Logger::ProcessBatch() {
        bool timeout = (core::MonoClock::now() - last_flush_) >= BATCH_TIMEOUT;
        if (entries_ > 0 && timeout) {
            Flush();
            last_flush_ = core::MonoClock::now();
        }

        auto entry = Pop();
        if(entry == nullptr) {
            return;
        }

        batch_[entries_] = std::move(entry);
        entries_++;
        if (entries_ >= ENTRY_BATCH_SIZE) {
            Flush();
            last_flush_ = core::MonoClock::now();
        }
    }

    void Logger::Run(std::stop_token token) {
        while (running_.load(std::memory_order_acquire) || !token.stop_requested()) {
            ProcessBatch();
            core::sleep_ms(5u);
        }
        if(entries_ > 0) {
            Flush();
        }
    }

    void Logger::Start() {
        bool expected { false };
        if (running_.compare_exchange_strong(expected, true)) {
            return;
        }
        worker_handle_ = std::jthread([this](auto token) { this->Run(token); });
    }

    Logger::~Logger() {
        StopAndFlush();
    }
}



