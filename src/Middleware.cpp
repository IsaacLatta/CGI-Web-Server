#include "Middleware.h"
#include "Session.h"

asio::awaitable<void> ErrorHandlerMiddleware::process(Transaction* txn, Next next) {
    try {
        LOG("DEBUG", "ErrorHandlerMW", "processed");
        co_await next();
    }
    catch (const http::HTTPException& http_error) {
        txn->log_entry.response = http_error.getResponse()->getStr();
        txn->sock->write(txn->log_entry.response.data(), txn->log_entry.response.size());
        logger::log_session(txn->log_entry, logger::INFO);
    }
    catch (const std::exception& error) {
        txn->log_entry.response = http::get_status_msg(http::code::Internal_Server_Error);
        txn->sock->write(txn->log_entry.response.data(), txn->log_entry.response.size());
        logger::log_session(txn->log_entry, logger::INFO);
    }
}

std::unique_ptr<MethodHandler> RequestHandlerMiddleware::createMethodHandler(const std::vector<char>& buffer, Transaction* txn) {
    std::string_view request(buffer.data(), buffer.size());
    if(http::trim_to_upper(request).find(http::method::GET) != std::string::npos) {
        LOG("INFO", "Request Handler", "GET request detected");
        return std::make_unique<GetHandler>(txn->getSocket(), buffer.data(), buffer.size()); 
    }
    if(http::trim_to_upper(request).find(http::method::HEAD) != std::string::npos) {
        LOG("INFO", "Request Handler", "HEAD request detected");
        return std::make_unique<HeadHandler>(txn->getSocket(), buffer.data(), buffer.size()); 
    }
    if(http::trim_to_upper(request).find(http::method::POST) != std::string::npos) {
        LOG("INFO", "Request Handler", "POST request detected");
        return std::make_unique<PostHandler>(txn->getSocket(), buffer.data(), buffer.size());
    }
    return nullptr;
}

void log_parsed_results(const http::Request& request) {
    LOG("DEBUG", "ParserMW", "PARSED RESULTS\n\tMethod: %s\n\tEndpoint: %s\n\tBody: %s", 
    request.method.c_str(), request.endpoint.c_str(), request.body.c_str());

    for(const auto [key, val]: request.headers) {
        LOG("DEBUG", "ParserMW Header Found", "%s: %s", key.c_str(), val.c_str()); 
    }
}

asio::awaitable<void> ParserMiddleware::process(Transaction* txn, Next next) {

    std::vector<char> buffer(HEADER_SIZE);
    auto [ec, bytes] = co_await txn->getSocket()->co_read(buffer.data(), buffer.size());
    if(ec) {
        throw (ec.value() == asio::error::connection_reset || ec.value() == asio::error::broken_pipe || ec.value() == asio::error::eof) ?
                http::HTTPException(http::code::Client_Closed_Request, std::format("Failed to read request from client: {}", txn->sock->getIP())) :
                http::HTTPException(http::code::Internal_Server_Error, std::format("Failed to read request from client: {}", txn->sock->getIP()));
    }

    http::code code;
    http::Request request;
    http::Response* response = txn->getResponse();
    if((code = http::extract_method(buffer, request.method)) != http::code::OK || (code = http::extract_endpoint(buffer, request.endpoint)) != http::code::OK || 
       (code = http::extract_body(buffer, request.body)) != http::code::OK || (code = http::extract_headers(buffer, request.headers)) != http::code::OK) {
    
        log_parsed_results(request);
        throw http::HTTPException(code, std::format("Failed to parse request for client: {}\nResults\n\tMethod: {}\n\tEndpoint: {}\n\tBody: {}", 
        txn->getSocket()->getIP(), request.method, request.endpoint, request.body));
    }
    
    log_parsed_results(request);
    txn->setRequest(std::move(request));
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
    entry->response = txn->getResponse()->getStr();
    entry->end_time = std::chrono::system_clock::now();
    logger::log_session(*entry, "INFO");
}

asio::awaitable<void> RequestHandlerMiddleware::process(Transaction* txn, Next next) {
    LOG("DEBUG", "RequestHandlerMW", "processed");
    if(auto handler = createMethodHandler(buffer, txn)) {
        co_await handler->handle();
        co_await next();
    }
    else {
        throw http::HTTPException(http::code::Not_Implemented, "Request method not supported, supported methods (GET, HEAD, POST)");
    }    
    
    co_await next();
}