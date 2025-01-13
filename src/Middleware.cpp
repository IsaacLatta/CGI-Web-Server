#include "Middleware.h"
#include "Session.h"

using namespace mw;

asio::awaitable<void> mw::ErrorHandler::process(Transaction* txn, Next next) {
    try {
        co_await next();
        if(txn->finish) {
            co_await txn->finish();
        }
    }
    catch (const http::HTTPException& http_error) {
        txn->response = std::move(*http_error.getResponse());
        std::string response = txn->response.build();
        txn->sock->write(response.data(), response.length());
        LOG("ERROR", "Error Handler", "ERROR MESSAGE: %s", http_error.what());
    }
    catch (const std::exception& error) {
        txn->response = std::move(http::Response(http::code::Internal_Server_Error));
        std::string response = txn->response.build();
        txn->sock->write(response.data(), response.length());
        LOG("ERROR", "Error Handler", "ERROR MESSAGE: %s", error.what());
    }
    co_return;
}

asio::awaitable<void> mw::Parser::process(Transaction* txn, Next next) {
    auto buffer =  txn->getBuffer();
    auto [ec, bytes] = co_await txn->getSocket()->co_read(buffer->data(), buffer->size());
    txn->getLogEntry()->Latency_end_time = std::chrono::system_clock::now();
    buffer->resize(bytes);

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
    request.route = config->findRoute(request.endpoint);
    
    txn->setRequest(std::move(request));
    co_await next();
}

asio::awaitable<void> mw::Logger::process(Transaction* txn, Next next) {
    logger::Entry* entry = txn->getLogEntry();
    entry->Latency_start_time = std::chrono::system_clock::now();
    entry->RTT_start_time = std::chrono::system_clock::now();
    entry->client_addr = txn->getSocket()->getIP();
    
    co_await next();

    const std::vector<char>* buffer = txn->getBuffer();
    entry->user_agent = logger::get_user_agent(buffer->data(), buffer->size());
    entry->request = logger::get_header_line(buffer->data(), buffer->size());
    entry->response = txn->getResponse()->status_msg;
    entry->RTT_end_time = std::chrono::system_clock::now();
    logger::log_session(*entry, std::string(http::is_success_code(txn->getResponse()->status) ? logger::INFO : logger::ERROR));
}

std::shared_ptr<MethodHandler> mw::RequestHandler::createMethodHandler(Transaction* txn) {
    http::Request* request = txn->getRequest();
    if(request->method == http::method::GET) {
        return std::make_shared<GetHandler>(txn); 
    }
    if(request->method == http::method::HEAD) {
        return std::make_shared<HeadHandler>(txn); 
    }
    if(request->method == http::method::POST) {
        return std::make_shared<PostHandler>(txn);
    }
    return nullptr;
}

asio::awaitable<void> mw::RequestHandler::process(Transaction* txn, Next next) {
    if(auto handler = createMethodHandler(txn)) {
        co_await handler->handle();
        co_await next();
    }
    else {
        throw http::HTTPException(http::code::Not_Implemented, "Request method not supported, supported methods (GET, HEAD, POST)");
    }    
}

void mw::Authenticator::validate(Transaction* txn, const cfg::Route* route) {
    auto request = txn->getRequest();

    if(!route->is_protected) {
        return;
    }

    std::string cookie, token;
    if ((cookie = request->getHeader("Cookie")).empty() || (token = http::extract_jwt_from_cookie(cookie)).empty()) {
        throw http::HTTPException(http::code::Unauthorized, "Missing or invalid authentication token");
    }

    try {
        auto decoded_token = jwt::decode(token);
        auto verifier = jwt::verify().allow_algorithm(jwt::algorithm::hs256{config->getSecret()}).with_issuer(config->getServerName());
        verifier.verify(decoded_token);

        if(decoded_token.has_expires_at() && std::chrono::system_clock::now() > decoded_token.get_expires_at()) {
            throw http::HTTPException(http::code::Unauthorized, "Expired token");
        }

        auto role_claim = decoded_token.get_payload_claim("role");
        const cfg::Role* role;
        if(!((role = config->findRole(role_claim.as_string())) && role->includesRole(route->role))) {
            throw http::HTTPException(http::code::Unauthorized, "Insufficient permissions");
        }
    } catch (const std::exception& error) {
        throw http::HTTPException(http::code::Unauthorized, 
        std::format("[client {}] Invalid token [error {}]", txn->getSocket()->getIP(), error.what()));
    }
}

asio::awaitable<void> mw::Authenticator::process(Transaction* txn, Next next) {
    auto request = txn->getRequest();
    const cfg::Config* config = cfg::Config::getInstance();
   
    if(!request->route || !request->route->is_protected) {
        co_await next();
        co_return;
    }

    validate(txn, request->route);
    co_await next();
    auto response = txn->getResponse();
    
    if(request->route->is_authenticator) { 
        if(!http::is_success_code(response->status)) {
            throw http::HTTPException(response->status, std::format("Failed to authorize client: {} [status={}]", txn->getSocket()->getIP(), static_cast<int>(response->status)));
        }
        
        auto token_builder = jwt::create();
        std::string token = token_builder.set_issuer(config->getServerName()).set_subject("auth-token").set_expires_at(DEFAULT_EXPIRATION)
                            .set_payload_claim("role", jwt::claim(cfg::get_role_hash(request->route->role))).sign(jwt::algorithm::hs256{config->getSecret()});
        response->addHeader("Set-Cookie", std::format("jwt={}; HttpOnly; Secure; SameSite=Strict;", token));
    }
}
