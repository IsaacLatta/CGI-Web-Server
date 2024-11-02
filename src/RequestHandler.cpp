#include "RequestHandler.h"
#include "Session.h"

std::unique_ptr<RequestHandler> RequestHandler::handlerFactory(std::shared_ptr<Session> session, std::shared_ptr<std::vector<char>> buffer) {
    std::string request(buffer->data(), buffer->size());
    if(request.find("GET") != std::string::npos || request.find("get") != std::string::npos) {
        return std::make_unique<GetHandler>(session, session->getSocket(), buffer); 
    }
    else {
        return nullptr;
    }
}

void GetHandler::handle() {
    
}