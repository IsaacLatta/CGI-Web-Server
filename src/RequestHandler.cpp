#include "RequestHandler.h"
#include "Session.h"

std::unique_ptr<RequestHandler> RequestHandler::handlerFactory(std::weak_ptr<Session> sess, const char* buffer, std::size_t size) {
    auto session = sess.lock();
    if(!session) {
        ERROR("Get Handler", 0, "NULL", "failed to lock session observer");
        return nullptr;
    }
    
    std::string request(buffer, size);

    if(http::trim_to_lower(request).find("get") != std::string::npos) {
        LOG("INFO", "Request Handler", "GET request detected");
        return std::make_unique<GetHandler>(sess, session->getSocket(), buffer, size); 
    }
    if(http::trim_to_lower(request).find("head") != std::string::npos) {
        LOG("INFO", "Request Handler", "HEAD request detected");
        return std::make_unique<HeadHandler>(sess, session->getSocket(), buffer, size); 
    }
    if(http::trim_to_lower(request).find("post") != std::string::npos) {
        LOG("INFO", "Request Handler", "POST request detected");
        return std::make_unique<PostHandler>(sess, session->getSocket(), buffer, size);
    }

    session->onError(http::error(http::code::Not_Implemented, "Request Method not supported"));
    return nullptr;
}

std::optional<http::error> RequestHandler::authenticate(const cfg::Route* route) {
    if(!route->is_protected) {
        return std::nullopt;
    }
    
    http::code code;
    std::string token_recv;
    if((code = http::extract_token(buffer, token_recv)) != http::code::OK) {
        return http::error(code, 
        std::format("Failed to authenticate token for protected endpoint {} with POST, required role {}", route->endpoint, route->role)); 
    } 

    LOG("INFO", "authenticator", "TOKEN EXTRACTED: %s", token_recv.c_str());
    try {
        auto decoded_token = jwt::decode(token_recv);
        LOG("INFO", "authenticator", "Header: %s", decoded_token.get_header().c_str());
        LOG("INFO", "authenticator", "Payload: %s", decoded_token.get_payload().c_str());
        LOG("INFO", "authenticator", "Algorithm: %s", decoded_token.get_algorithm().c_str());

        auto verifier = jwt::verify();
        auto algo = decoded_token.get_algorithm();
        if (algo == "HS256") {
            verifier.allow_algorithm(jwt::algorithm::hs256(config->getSecret()));
        }
        else if (algo == "HS512") {
            verifier.allow_algorithm(jwt::algorithm::hs512(config->getSecret()));
        }
        else {
            throw std::runtime_error(std::format("Token recv for endpoint: {}, unsupported algorithm: {}", route->endpoint, algo));
        }

        verifier.with_issuer(config->getHostName()).verify(decoded_token);

        return std::nullopt;
    }
    catch (const std::exception &e) {
        return http::error(http::code::Bad_Request, e.what());
    }
}

