#include "config.h"

using namespace cfg;

Config Config::INSTANCE;
std::once_flag Config::initFlag;

Config::Config() {}

const Config* Config::getInstance(const std::string& config_path) {
    if(!config_path.empty()) {
        std::call_once(Config::initFlag, [config_path]() {Config::INSTANCE.initialize(config_path);});
    }
    return &Config::INSTANCE;
}

void Config::loadRoutes(tinyxml2::XMLDocument* doc, const std::string& content_path) {
    using namespace tinyxml2;
    using namespace cfg;
    
    XMLElement* doc_routes = doc->FirstChildElement("ServerConfig")->FirstChildElement("Routes");
    if(!doc_routes) {
        return;
    }

    std::string is_protected;
    XMLElement* route_el = doc_routes->FirstChildElement("Route");
    while(route_el) {
        Route route;
        route.method = route_el->Attribute("method") ? route_el->Attribute("method") : "";
        route.endpoint = route_el->Attribute("endpoint") ? route_el->Attribute("endpoint") : "";
        route.script = route_el->Attribute("script") ? content_path + "/" + route_el->Attribute("script") : "";
        route.is_protected = route_el->Attribute("protected") && std::string(route_el->Attribute("protected")) == "true";
        if(route.is_protected) {
            route.role = route_el->Attribute("role") ? route_el->Attribute("role") : user; // default to user for protected routes
        }
        else {
            route.role = viewer;
        }
        route.is_authenticator = route_el->Attribute("authenticator") && std::string(route_el->Attribute("authenticator")) == "true";
                    
        if (!route.method.empty() && !route.endpoint.empty()) {
                routes[route.endpoint] = route;
            } 
        else {
            ERROR("loading config", 0, "incomplete route attribute", ""); 
        }
        route_el = route_el->NextSiblingElement("Route");
    }
}

const Route* Config::findRoute(const Endpoint& endpoint) const {
    auto it = routes.find(endpoint);
    if(it == routes.end()) {
        return nullptr;
    }
    return &it->second;
}

void Config::printRoutes() const {
    for (const auto& [endpoint, route] : routes) {
        std::cout << "Endpoint: " << endpoint << "\n";
        std::cout << "  Method: " << route.method << "\n";
        std::cout << "  Script: " << (route.script.empty() ? "Empty" : route.script) << "\n";
        std::cout << "  Protected: " << (route.is_protected ? "Yes" : "No") << "\n";
        std::cout << "  Role: " << route.role << "\n";
        std::cout << "  Authenticator: " << (route.is_authenticator ? "Yes" : "No") << "\n";
        std::cout << "\n";
    }
}

void Config::initialize(const std::string& config_path) {
    tinyxml2::XMLDocument doc;
    
    if(doc.LoadFile(config_path.c_str()) != tinyxml2::XML_SUCCESS) {
        logger::log_message("FATAL", "Server", std::format("Configuration Failed [error={} {}]", static_cast<int>(doc.ErrorID()), doc.ErrorStr()));
        EXIT_FATAL("loading server configuration", static_cast<int>(doc.ErrorID()), doc.ErrorStr(), "failed to load config file: %s", config_path.c_str());
    }

    tinyxml2::XMLElement* web_dir = doc.FirstChildElement("ServerConfig")->FirstChildElement("WebDirectory");
    if(web_dir) {
        content_path = web_dir->GetText();
    }
    else {
        logger::log_message("FATAL", "Server", "Configuration Failed: no web directory path found");
        EXIT_FATAL("loading server configuration", 0, "no web directory path", "please provide a path to serve from");
    }

    loadRoutes(&doc, content_path);
}

std::string cfg::getRoleHash(const std::string& role) {
    return role;
}
