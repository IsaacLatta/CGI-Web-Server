#include "Middleware.h"
#include "Session.h"

asio::awaitable<void> ErrorHandlerMiddleware::process(Transaction* txn, Next next) {
    try {
        LOG("DEBUG", "ErrorHandlerMW", "processed");
        co_await next();
        if(txn->finish) {
            co_await txn->finish();
        }
    }
    catch (const http::HTTPException& http_error) {
        txn->response = std::move(*http_error.getResponse());
        txn->sock->write(txn->response.status_msg.data(), txn->response.status_msg.length());
        LOG("ERROR", "Error Handler", "ERROR MESSAGE: %s", http_error.what());
    }
    catch (const std::exception& error) {
        txn->response = std::move(http::Response(http::code::Internal_Server_Error));
        txn->sock->write(txn->response.status_msg.data(), txn->response.status_msg.length());
        LOG("ERROR", "Error Handler", "ERROR MESSAGE: %s", error.what());
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
    auto buffer =  txn->getBuffer();
    auto [ec, bytes] = co_await txn->getSocket()->co_read(buffer->data(), buffer->size());
    txn->getLogEntry()->Latency_end_time = std::chrono::system_clock::now();

    if(ec) {
        throw (ec.value() == asio::error::connection_reset || ec.value() == asio::error::broken_pipe || ec.value() == asio::error::eof) ?
                http::HTTPException(http::code::Client_Closed_Request, std::format("Failed to read request from client: {}", txn->sock->getIP())) :
                http::HTTPException(http::code::Internal_Server_Error, std::format("Failed to read request from client: {}", txn->sock->getIP()));
    }

    http::code code;
    http::Request request;
    if((code = http::extract_method(*buffer, request.method)) != http::code::OK || (code = http::extract_endpoint(*buffer, request.endpoint)) != http::code::OK || 
       (code = http::extract_body(*buffer, request.body)) != http::code::OK || (code = http::extract_headers(*buffer, request.headers)) != http::code::OK) {
    
        throw http::HTTPException(code, std::format("Failed to parse request for client: {}\nResults\n\tMethod: {}\n\tEndpoint: {}\n\tBody: {}", 
        txn->getSocket()->getIP(), request.method, request.endpoint, request.body));
    }
    
    txn->setRequest(std::move(request));
    co_await next();
}

asio::awaitable<void> LoggingMiddleware::process(Transaction* txn, Next next) {
    logger::Entry* entry = txn->getLogEntry();
    LOG("DEBUG", "LoggingMW", "processing forward");
    entry->Latency_start_time = std::chrono::system_clock::now();
    entry->RTT_start_time = std::chrono::system_clock::now();
    entry->RTT_start_time = std::chrono::system_clock::now();
    entry->client_addr = txn->getSocket()->getIP();
    
    co_await next();
    LOG("DEBUG", "LoggingMW", "processing backward");

    const std::vector<char>* buffer = txn->getBuffer();
    entry->user_agent = logger::get_user_agent(buffer->data(), buffer->size());
    entry->request = logger::get_header_line(buffer->data(), buffer->size());
    entry->response = txn->getResponse()->status_msg;
    entry->RTT_end_time = std::chrono::system_clock::now();
    logger::log_session(*entry, "INFO");
}

std::shared_ptr<MethodHandler> RequestHandlerMiddleware::createMethodHandler(Transaction* txn) {
    http::Request* request = txn->getRequest();
    if(request->method == http::method::GET) {
        LOG("INFO", "Request Handler", "GET request detected");
        return std::make_shared<GetHandler>(txn); 
    }
    if(request->method == http::method::HEAD) {
        LOG("INFO", "Request Handler", "HEAD request detected");
        return std::make_shared<HeadHandler>(txn); 
    }
    if(request->method == http::method::POST) {
        LOG("INFO", "Request Handler", "POST request detected");
        return std::make_shared<PostHandler>(txn);
    }
    return nullptr;
}

asio::awaitable<void> RequestHandlerMiddleware::process(Transaction* txn, Next next) {
    if(auto handler = createMethodHandler(txn)) {
        co_await handler->handle();
        LOG("DEBUG", "RequestHandlerMW", "processed");
        co_await next();
    }
    else {
        throw http::HTTPException(http::code::Not_Implemented, "Request method not supported, supported methods (GET, HEAD, POST)");
    }    
}


/*
    if (active_route->is_authenticator)
    {
        LOG("INFO", "POST handler", "authenticating user");
        auto tokenBuilder = jwt::create();
        token = tokenBuilder
                    .set_issuer(config->getServerName())
                    .set_subject("auth-token")
                    .set_payload_claim("role", jwt::claim(cfg::getRoleHash(active_route->role)))
                    .set_expires_at(DEFAULT_EXPIRATION)
                    .sign(jwt::algorithm::hs256{config->getSecret()});
        response_body["token"] = token;
    }
    else {
        response_body["message"] = "Endpoint active, no script set";
    }
    */


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
