#include "Middleware.h"
#include "Session.h"

using namespace mw;

asio::awaitable<void> mw::ErrorHandler::process(Transaction* txn, Next next) {
    http::Handler error_handler = nullptr;
    try {
        co_await next();
        auto request = txn->getRequest();
        if(auto finisher = request->endpoint->getHandler(request->method)) {
            co_await finisher(txn);
        }
    }
    catch (const http::HTTPException& http_error) {
        DEBUG("MW Error Handler", "status=%d %s", static_cast<int>(http_error.getResponse()->getStatus()), http_error.what());
        txn->response = std::move(*http_error.getResponse());
        error_handler = http::Router::getInstance()->getErrorPage(txn->response.getStatus())->handler;
    }
    catch (const std::exception& error) {
        DEBUG("MW Error Handler", "status=500, std exception: %s", error.what());
        txn->response = std::move(http::Response(http::code::Internal_Server_Error));
        error_handler = http::Router::getInstance()->getErrorPage(txn->response.getStatus())->handler;
    }

    if(error_handler) {
        TRACE("MW Error Handler", "Serving error page for status=%d", static_cast<int>(txn->response.getStatus()));
        co_await error_handler(txn);
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

    auto router = http::Router::getInstance();
    http::Request request;
    request.endpoint_url = http::extract_endpoint(*buffer);
    request.endpoint = router->getEndpoint(request.endpoint_url);
    request.method = http::extract_method(*buffer);
    request.args = http::extract_args(*buffer, request.endpoint->getArgType(request.method));
    request.headers = http::extract_headers(*buffer);
    request.body = http::extract_body(*buffer);
    request.route = router->getEndpointMethod(request.endpoint_url, request.method);

    TRACE("MW Parser", "Hit for endpoint: %s", request.endpoint_url.c_str());

    txn->setRequest(std::move(request));
    co_await next();
}

asio::awaitable<void> mw::Logger::process(Transaction* txn, Next next) {
    logger::SessionEntry* entry = txn->getLogEntry();
    entry->Latency_start_time = std::chrono::system_clock::now();
    entry->RTT_start_time = std::chrono::system_clock::now();
    entry->client_addr = txn->getSocket()->getIP();
    
    co_await next();

    const std::vector<char>* buffer = txn->getBuffer();
    entry->user_agent = logger::get_user_agent(buffer->data(), buffer->size());
    entry->request = logger::get_header_line(buffer->data(), buffer->size());
    entry->response = txn->getResponse()->status_msg;
    entry->RTT_end_time = std::chrono::system_clock::now();
    entry->level = http::is_success_code(txn->getResponse()->status) ? logger::level::Info : logger::level::Error;
    LOG_SESSION(std::move(txn->log_entry));
}

void mw::Authenticator::validate(Transaction* txn, const http::EndpointMethod* route) {
    auto request = txn->getRequest();

    if(!route->is_protected) {
        return;
    }

    std::string cookie, token;
    if ((cookie = request->getHeader("Cookie")).empty() || (token = http::extract_jwt_from_cookie(cookie)).empty()) {
        throw http::HTTPException(http::code::Unauthorized, "missing or invalid authentication token");
    }

    try {
        auto decoded_token = jwt::decode(token);
        auto verifier = jwt::verify().allow_algorithm(jwt::algorithm::hs256{config->getSecret()}).with_issuer(config->getServerName());
        verifier.verify(decoded_token);

        if(decoded_token.has_expires_at() && std::chrono::system_clock::now() > decoded_token.get_expires_at()) {
            throw http::HTTPException(http::code::Unauthorized, "expired token");
        }

        auto role_claim = decoded_token.get_payload_claim("role");
        const cfg::Role* role;
        if(!((role = config->findRole(role_claim.as_string())) && role->includesRole(route->access_role))) {
            throw http::HTTPException(http::code::Unauthorized, "insufficient permissions");
        }
    } catch (const std::exception& error) {
        throw http::HTTPException(http::code::Unauthorized, 
        std::format("[client {}] invalid token [error {}]", txn->getSocket()->getIP(), error.what()));
    }
}

asio::awaitable<void> mw::Authenticator::process(Transaction* txn, Next next) {
    auto request = txn->getRequest();
    const cfg::Config* config = cfg::Config::getInstance();
 
    validate(txn, request->route);
    co_await next();
    
     if(!request->route->is_authenticator) { 
        co_return;
    }

    auto response = txn->getResponse();
    if(!http::is_success_code(response->status)) {
        throw http::HTTPException(response->status, std::format("Failed to authorize client: {} [status={}]", txn->getSocket()->getIP(), static_cast<int>(response->status)));
    }
    
    auto token_builder = jwt::create();
    std::string token = token_builder.set_issuer(config->getServerName()).set_subject("auth-token").set_expires_at(DEFAULT_EXPIRATION)
                        .set_payload_claim("role", jwt::claim(cfg::get_role_hash(request->endpoint->getAuthRole(request->method)))).sign(jwt::algorithm::hs256{config->getSecret()});
    response->addHeader("Set-Cookie", std::format("jwt={}; HttpOnly; Secure; SameSite=Strict;", token));
    co_return;
}
 