#include "Middleware.h"
#include "Session.h"


/* Add http exception class later */
asio::awaitable<void> ErrorHandlerMiddleware::process(Transaction* txn, Next next) {
    try {
        LOG("DEBUG", "ErrorHandlerMW", "processed");
        co_await next();
    }
    catch (const std::exception& error) {
        // log the error here, could invoke retries, send error response etc
    }
}

asio::awaitable<void> ParserMiddleware::process(Transaction* txn, Next next) {

    std::vector<char> buffer(HEADER_SIZE);
    auto [ec, bytes] = co_await txn->getSocket()->co_read(buffer.data(), buffer.size());
    if(ec) {
        // throw http error here
    }
    LOG("DEBUG", "ParserMW", "request: %s", buffer.data());
    co_await next();
}
    

asio::awaitable<void> LoggingMiddleware::process(Transaction* txn, Next next) {
    logger::Entry* entry = txn->getLogEntry();
    LOG("DEBUG", "LoggingMW", "processed");
    if(forward) {
        const std::vector<char>* buffer = txn->getBuffer();
        entry->user_agent = logger::get_user_agent(buffer->data(), buffer->size());
        entry->request = logger::get_header_line(buffer->data(), buffer->size());
        entry->RTT_start_time = std::chrono::system_clock::now();
        entry->client_addr = txn->getSocket()->getIP();
        forward = false;
        co_await next();
        co_return;
    }

    entry->bytes = txn->getBytes();
    entry->response = txn->getResponse()->built_response;
    entry->end_time = std::chrono::system_clock::now();
    logger::log_session(*entry, "INFO");
}

asio::awaitable<void> RequestHandlerMiddleware::process(Transaction* txn, Next next) {
    LOG("DEBUG", "RequestHandlerMW", "processed");
    co_await next();
}