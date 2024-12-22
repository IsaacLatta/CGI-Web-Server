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
        LOG("ERROR", "Error Handler", "ERROR MESSAGE: %s", http_error.what());
        logger::log_session(txn->log_entry, logger::ERROR);
    }
    catch (const std::exception& error) {
        txn->log_entry.response = http::get_status_msg(http::code::Internal_Server_Error);
        txn->sock->write(txn->log_entry.response.data(), txn->log_entry.response.size());
        LOG("ERROR", "Error Handler", "ERROR MESSAGE: %s", error.what());
        logger::log_session(txn->log_entry, logger::ERROR);
    }
}

void log_parsed_results(const http::Request& request) {
    LOG("DEBUG", "ParserMW", "PARSED RESULTS\n\tMethod: %s\n\tEndpoint: %s\n\tBody: %s\nHeaders Size: %d", 
    request.method.c_str(), request.endpoint.c_str(), request.body.c_str(), static_cast<int>(request.headers.size()));

    for(const auto& [key, val]: request.headers) {
        std::cout << "\tPARSED HEADER -> " << key << ": " << val << "\n";
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
    if((code = http::extract_method(buffer, request.method)) != http::code::OK || (code = http::extract_endpoint(buffer, request.endpoint)) != http::code::OK || 
       (code = http::extract_body(buffer, request.body)) != http::code::OK || (code = http::extract_headers(buffer, request.headers)) != http::code::OK) {
    
        throw http::HTTPException(code, std::format("Failed to parse request for client: {}\nResults\n\tMethod: {}\n\tEndpoint: {}\n\tBody: {}", 
        txn->getSocket()->getIP(), request.method, request.endpoint, request.body));
    }
    
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

std::unique_ptr<MethodHandler> RequestHandlerMiddleware::createMethodHandler(Transaction* txn) {
    http::Request* request = txn->getRequest();
    if(request->method == http::method::GET) {
        LOG("INFO", "Request Handler", "GET request detected");
        return std::make_unique<GetHandler>(txn->getSocket(), txn->getRequest(), txn->getResponse()); 
    }
    if(request->method == http::method::HEAD) {
        LOG("INFO", "Request Handler", "HEAD request detected");
        return std::make_unique<HeadHandler>(txn->getSocket(), txn->getRequest(), txn->getResponse()); 
    }
    if(request->method == http::method::POST) {
        LOG("INFO", "Request Handler", "POST request detected");
        return std::make_unique<PostHandler>(txn->getSocket(), txn->getRequest(), txn->getResponse());
    }
    return nullptr;
}

asio::awaitable<void> RequestHandlerMiddleware::process(Transaction* txn, Next next) {
    LOG("DEBUG", "RequestHandlerMW", "processed");
    
    if(auto handler = createMethodHandler(txn)) {
        co_await handler->handle();
        co_await next();
    }
    else {
        throw http::HTTPException(http::code::Not_Implemented, "Request method not supported, supported methods (GET, HEAD, POST)");
    }    
}

asio::awaitable<void> AuthenticatorMiddleware::process(Transaction* txn, Next next) {
    LOG("DEBUG", "AuthenticatorMW", "processed");
    auto request = txn->getRequest();
    const cfg::Config* config = cfg::Config::getInstance();

    const cfg::Route* route = config->findRoute(request->endpoint);
    if(route && route->is_protected) {
        // Authenticate
        co_await next();
        co_return;
    }
    request->endpoint = "public/" + request->endpoint;
    co_await next();
}
