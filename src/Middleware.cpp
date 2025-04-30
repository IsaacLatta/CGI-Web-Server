#include "Middleware.h"
#include "Session.h"

using namespace mw;

static asio::awaitable<void> run_pipeline(Transaction *txn, std::size_t index, std::vector<std::unique_ptr<Middleware>>* pipeline) {
    if (index >= pipeline->size()) {
        co_return;
    }

    mw::Middleware *mw = (*pipeline)[index].get();
    mw::Next next_func = [pipeline, txn, index]() -> asio::awaitable<void> {
        co_await run_pipeline(txn, index + 1, pipeline);
    };
    co_await mw->process(txn, next_func);
}

asio::awaitable<void> Pipeline::run(Transaction* txn) {
    co_return co_await run_pipeline(txn, 0, &components);
}

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

IpInfo* IPRateLimiter::findClient(const std::string& ip) {
    auto it = clients.find(ip);
    if(it != clients.end()) {
        return it->second.get();
    } 

    std::lock_guard<std::mutex> lock(clients_mutex);
    it = clients.find(ip);
    if(it != clients.end()) {
        return it->second.get();
    }
    auto client_ptr = std::make_unique<IpInfo>();
    auto client_raw = client_ptr.get();
    clients.emplace(ip, std::move(client_ptr));
    return client_raw;
}

asio::awaitable<void> mw::IPRateLimiter::process(Transaction* txn, Next next) {
    std::string ip = txn->getSocket()->getIP();
    auto client_info = findClient(ip);

    auto now = std::chrono::steady_clock::now();
    auto secs = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
    std::uint32_t this_window_id = secs / setting.window_seconds;
    std::uint64_t desired;
    std::uint64_t old = client_info->window_and_count.load(std::memory_order_relaxed);
    do {
        std::uint32_t old_window_id = std::uint32_t(old >> 32); 
        std::uint32_t old_count = std::uint32_t(old);
        
        if(old_window_id == this_window_id) {
            if(old_count >= setting.max_requests) {
                throw http::HTTPException(http::code::Too_Many_Requests, 
                std::format("client={} has exceeded {} requests in {}s", ip, setting.max_requests, setting.window_seconds));
            } 
            desired = (std::uint64_t(this_window_id) << 32) | (old_count + 1); // increment by 1
        } else {
            desired = (std::uint64_t(this_window_id) << 32) | 1u; // reset to 1
        }
    } while(client_info->window_and_count.compare_exchange_weak(old, desired, std::memory_order_relaxed, std::memory_order_relaxed));

    co_await next();
}

Bucket* mw::TokenBucketLimiter::findBucket(const std::string& key) {
    auto it = buckets.find(key);
    if(it != buckets.end()) {
        return it->second.get();
    }

    std::lock_guard lock(buckets_mutex);
    it = buckets.find(key);
    if(it != buckets.end()) {
        return it->second.get();
    }

    auto new_bucket = std::make_unique<Bucket>();

    auto now_s = std::uint32_t(std::chrono::duration_cast<std::chrono::seconds>
                (std::chrono::steady_clock::now().time_since_epoch()).count());
    std::uint64_t init = (std::uint64_t(setting.capacity) << 32) | now_s;
    new_bucket->tokens_and_refill.store(init, std::memory_order_relaxed);  

    auto raw = new_bucket.get();
    buckets.emplace(key, std::move(new_bucket));
    return raw;
}

asio::awaitable<void> mw::TokenBucketLimiter::process(Transaction* txn, Next next) {
    std::string key = setting.make_key(txn);
    auto bucket = findBucket(key);

    auto now = std::chrono::steady_clock::now();
    auto secs = std::uint32_t(std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count());

    std::uint32_t updated_tokens, new_refill;
    std::uint64_t desired;
    std::uint64_t old = bucket->tokens_and_refill.load(std::memory_order_relaxed);
    do {
        std::uint32_t old_tokens = std::uint32_t(old >> 32); // extract upper 32
        std::uint32_t old_refill = std::uint32_t(old); // extract lower 32
        std::uint32_t elapsed = secs > old_refill ? secs - old_refill : 0; // check on multiple req/s, overflow on secs
        std::uint64_t new_tokens = old_tokens + setting.refill_rate*elapsed;
        if(new_tokens > setting.capacity) {
            new_tokens = setting.capacity;
        }

        new_refill = elapsed > 0 ? secs : old_refill; // has a second passed ?

        if(new_tokens < 1) {
            std::unordered_map<std::string, std::string> headers;
            std::uint32_t retry_after = new_refill + 1 > secs ? (new_refill + 1 - secs) : 0;
            headers["Retry-After"] = std::to_string(retry_after); 
            throw http::HTTPException(http::code::Too_Many_Requests, 
                std::format("client={} has exceeded rate limit on [{} {}] ({} req/s cap={})", 
                txn->getSocket()->getIP(), http::method_enum_to_str(txn->getRequest()->method), 
                txn->getRequest()->endpoint_url, setting.refill_rate, setting.capacity), std::move(headers));
        }

        updated_tokens = std::uint32_t(new_tokens - 1);
        desired = (std::uint64_t(updated_tokens) << 32) | new_refill;
    } while(!bucket->tokens_and_refill.compare_exchange_weak(old, desired, std::memory_order_relaxed, std::memory_order_relaxed));


    auto response = txn->getResponse();
    response->addHeader("X-RateLimit-Limit", std::to_string(setting.capacity));
    response->addHeader("X-RateLimit-Remaining", std::to_string(updated_tokens));
    response->addHeader("X-RateLimit-Reset", std::to_string(new_refill + 1));

    co_await next();
}

 