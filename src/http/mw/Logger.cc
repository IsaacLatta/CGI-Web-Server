#include "http/mw/Logger.h"
#include "logger_macros.h"

namespace mw {

    asio::awaitable<void> mw::Logger::Process(http::Transaction& txn, Next next) {
        logger::SessionEntry& entry = txn.GetLogEntry();
        entry.RttStart = std::chrono::system_clock::now();
        entry.RemoteIpPortString = txn.GetSocket().IpStr();

        co_await next();

        // Reparses here since the error handler may served a different response than
        // present in the Transaction.

        entry.UserAgent = logger::get_user_agent(txn.GetBuffer());
        entry.RequestLine = logger::get_header_line(txn.GetBuffer());
        entry.ResponseLine = http::get_status_msg(txn.GetResponse().Status);
        entry.RttEnd = std::chrono::system_clock::now();
        entry.level = http::is_success_code(txn.GetResponse().Status) ? logger::level::Info : logger::level::Error;
        LOG_SESSION(entry);
    }

}