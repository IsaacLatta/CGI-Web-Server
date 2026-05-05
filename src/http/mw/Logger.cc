#include "http/mw/Logger.h"
#include "logger/macros.h"
#include "http/parsing/parse.h"
#include "http/Transaction.h"

namespace mw {

    asio::awaitable<void> SessionBeginLogger::Process(http::PreRouteContext& context, Next next, Finish) {
        context.GetLogEntry().RttStart = std::chrono::system_clock::now();
        context.GetLogEntry().RemoteIpPortString = context.GetSocket().GetIpStr();
        co_await next(context);
    }

    asio::awaitable<void> SessionEndLogger::Process(http::FinalContext& context, Next next, Finish) {
        context.GetLogEntry().UserAgent = logger::get_user_agent(context.GetBuffer());
        context.GetLogEntry().RequestLine = logger::get_header_line(context.GetBuffer());
        context.GetLogEntry().ResponseLine = http::get_status_msg(context.GetResponse().Status);
        context.GetLogEntry().RttEnd = std::chrono::system_clock::now();
        context.GetLogEntry().level = http::is_success_code(context.GetResponse().Status) ? logger::Level::Info : logger::Level::Error;
        LOG_SESSION(context.GetLogEntry());
        co_await next(context);
    }

}