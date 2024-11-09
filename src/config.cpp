#include "config.h"

static void load_routes(tinyxml2::XMLDocument* doc, config::ServerConfig& serverConfig) {
    using namespace tinyxml2;
    using namespace config;
    
    XMLElement* routes = doc->FirstChildElement("ServerConfig")->FirstChildElement("Routes");
    if(!routes) {
        return;
    }

    XMLElement* route_el = routes->FirstChildElement("Route");
    while(route_el) {
        Route route;
        route.method = route_el->Attribute("method") ? route_el->Attribute("method") : "";
        route.endpoint = route_el->Attribute("endpoint") ? route_el->Attribute("endpoint") : "";
        route.script = route_el->Attribute("script") ? route_el->Attribute("script") : "";
        if (!route.method.empty() && !route.endpoint.empty() && !route.script.empty()) {
                LOG("INFO", "loading route configuration", "method: %s, endpoint: %s, script: %s", route.method.c_str(), route.endpoint.c_str(), route.script.c_str());
                serverConfig.routes[route.endpoint] = route;
            } 
        else {
            ERROR("loading config", 0, "incomplete route attribute", ""); 
        }
        route_el = route_el->NextSiblingElement("Route");
    }
}

void config::print_routes(const std::unordered_map<Endpoint, Route>& routes) {
    for (const auto& [endpoint, route] : routes) {
        std::cout << "Endpoint: " << endpoint << std::endl;
        std::cout << "  Method: " << route.method << std::endl;
        std::cout << "  Script: " << route.script << std::endl;
        std::cout << "  Protected: " << (route.is_protected ? "Yes" : "No") << std::endl;
        std::cout << std::endl;
    }
}

bool config::load_config(const std::string& config_path, config::ServerConfig& serverConfig) {
    tinyxml2::XMLDocument doc;
    
    if(doc.LoadFile(config_path.c_str()) != tinyxml2::XML_SUCCESS) {
        EXIT_FATAL("loading server configuration", static_cast<int>(doc.ErrorID()), doc.ErrorStr(), "failed to load config file: %s", config_path.c_str());
    }

    tinyxml2::XMLElement* web_dir = doc.FirstChildElement("ServerConfig")->FirstChildElement("WebDirectory");
    if(web_dir) {
        serverConfig.content_path = web_dir->GetText();
    }
    else {
        EXIT_FATAL("loading server configuration", 0, "no web directory path", "please provide a path to serve from");
    }

    load_routes(&doc, serverConfig);
    return true;
}