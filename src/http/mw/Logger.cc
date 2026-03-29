#include "http/mw/Logger.h"
#include "logger_macros.h"

namespace mw {

    asio::awaitable<void> mw::Logger::Process(Transaction& txn, Next next) {
        logger::SessionEntry* entry = txn.getLogEntry();
        entry->Latency_start_time = std::chrono::system_clock::now();
        entry->RTT_start_time = std::chrono::system_clock::now();
        entry->client_addr = txn.GetSocket()->IpStr();

        co_await next();

        const std::vector<char>* buffer = txn.getBuffer();
        entry->user_agent = logger::get_user_agent(buffer->data(), buffer->size());
        entry->request = logger::get_header_line(buffer->data(), buffer->size());
        entry->response = http::get_status_msg(txn.GetResponse().Status);
        entry->RTT_end_time = std::chrono::system_clock::now();
        entry->level = http::is_success_code(txn.GetResponse().Status) ? logger::level::Info : logger::level::Error;
        LOG_SESSION(std::move(txn.log_entry));
    }

}