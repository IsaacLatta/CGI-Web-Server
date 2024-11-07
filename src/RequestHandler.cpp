#include "RequestHandler.h"
#include "Session.h"

std::unique_ptr<RequestHandler> RequestHandler::handlerFactory(std::weak_ptr<Session> sess, const char* buffer, std::size_t size) {
    auto session = sess.lock();
    if(!session) {
        ERROR("Get Handler", 0, "NULL", "session observer is NULL");
        return nullptr;
    }
    
    std::string request(buffer, size);
    if(!session) {
        ERROR("shared_ptr.lock", 0, "NULL", "failed to lock session ptr");
        return nullptr;
    }

    if(request.find("GET") != std::string::npos || request.find("get") != std::string::npos) {
        LOG("INFO", "Request Handler", "Get request detected");
        return std::make_unique<GetHandler>(sess, session->getSocket(), buffer, size); 
    }
    return nullptr;
}
