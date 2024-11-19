#include "RequestHandler.h"
#include "Session.h"

std::unique_ptr<RequestHandler> RequestHandler::handlerFactory(std::weak_ptr<Session> sess, const char* buffer, std::size_t size) {
    auto session = sess.lock();
    if(!session) {
        ERROR("Get Handler", 0, "NULL", "failed to lock session observer");
        return nullptr;
    }
    
    std::string request(buffer, size);
    if(request.find("GET") != std::string::npos || request.find("get") != std::string::npos) {
        LOG("INFO", "Request Handler", "GET request detected");
        return std::make_unique<GetHandler>(sess, session->getSocket(), buffer, size); 
    }
    if(request.find("HEAD") != std::string::npos || request.find("head") != std::string::npos) {
        LOG("INFO", "Request Handler", "HEAD request detected");
        return std::make_unique<HeadHandler>(sess, session->getSocket(), buffer, size); 
    }
    if(request.find("POST") != std::string::npos || request.find("POST") != std::string::npos) {
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
    if((code = http::extract_header_field(buffer, "Authorization", token_recv)) != http::code::OK) {
        return http::error(code, 
        std::format("Failed to authenticate token for protected endpoint {} with POST, required role {}", route->endpoint, route->role)); 
    } 




    return std::nullopt;
}