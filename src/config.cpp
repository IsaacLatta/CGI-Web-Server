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
            route.role = route_el->Attribute("role") ? route_el->Attribute("role") : "";
            if (route.role.empty()) {
                // should exit
            }
        }
        else {
            route.role = VIEWER_ROLE_HASH;
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

const Role* Config::findRole(const std::string& role) const {
    auto it = roles.find(role);
    if(it == roles.end()) {
        return nullptr;
    }
    return &it->second;
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

void display_role(cfg::Role* role) {
    std::cout << "Title: " << role->title << "\n";
    for(const auto& include: role->includes) {
        std::cout << "  Include: " << include << "\n";
    }
    std::cout << "\n";
}

void Config::loadRoles(tinyxml2::XMLDocument* doc) {
    roles[ADMIN_ROLE_HASH] = ADMIN;
    roles[USER_ROLE_HASH] = USER;
    roles[VIEWER_ROLE_HASH] = VIEWER;

    tinyxml2::XMLElement* role_config = doc->FirstChildElement("ServerConfig")->FirstChildElement("Roles");
    if (!role_config) {
        return; 
    }

    std::vector<Role*> full_include_roles;
    tinyxml2::XMLElement* role_el = role_config->FirstChildElement("Role");
    while (role_el) {
        bool add_to_full_include_roles = false;

        const char* title_attr = role_el->Attribute("title");
        if (!title_attr || std::string(title_attr).empty()) {
            // Could generate random name or call EXIT_FATAL
        }

        std::string role_title = title_attr;
        Role role{get_role_hash(role_title), {}};

        tinyxml2::XMLElement* include_el = role_el->FirstChildElement("Includes");
        while (include_el) {
            const char* include_role = include_el->GetText();
            if (include_role && std::string(include_role) == "*") {
                add_to_full_include_roles = true;
                break; 
            } else if (include_role) {
                role.includes.push_back(get_role_hash(include_role));
            }
            include_el = include_el->NextSiblingElement("Includes");
        }

        roles[role.title] = role;
       
        if (add_to_full_include_roles) {
            full_include_roles.push_back(&roles[role.title]);
        }
        role_el = role_el->NextSiblingElement("Role");
    }

    for (auto role : full_include_roles) {
        for (const auto& [title, role_obj] : roles) {
            
            role->includes.push_back(title);
        }
    }
    for (auto [title, role]: roles) {
        display_role(&role);
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
        EXIT_FATAL("loading server configuration", 0, "no web directory path", "please provide a path to serve from");
    }

    if(chdir(content_path.c_str()) < 0) {
        EXIT_FATAL("loading server configuration", errno, strerror(errno), "failed to chdir to web directory path: %s", content_path.c_str());
    }

    logger::log_message("STATUS", "Server", "initialiizing ...");
    tinyxml2::XMLElement* host = doc.FirstChildElement("ServerConfig")->FirstChildElement("Host");
    if(host) {
        host_name = host->GetText();
    }
    else {
        host_name = cfg::NO_HOST_NAME;
    }

    tinyxml2::XMLElement* host_port = doc.FirstChildElement("ServerConfig")->FirstChildElement("Port");
    port = host_port ? std::stoi(host_port->GetText()) : 80;

    loadRoles(&doc);
    loadSSL(&doc);
    loadRoutes(&doc, content_path);
    loadHostIP();
}

std::string cfg::get_role_hash(std::string role) {
    return role;
}

void cfg::Config::loadSSL(tinyxml2::XMLDocument* doc) {
    tinyxml2::XMLElement* ssl_config = doc->FirstChildElement("ServerConfig")->FirstChildElement("SSL");
    if(!ssl_config) {
        return;
    }
    ssl.active = true;
    tinyxml2::XMLElement* cert = ssl_config->FirstChildElement("Certificate");
    if(!cert) {
        logger::log_message("FATAL", "Server", std::format("certificate not found: exiting pid={}", getpid()));
        EXIT_FATAL("loading server configuration", 0, "Certificate Not Found", "please provide a valid certificate");
    }
    ssl.certificate_path = cert->GetText();
    tinyxml2::XMLElement* key = ssl_config->FirstChildElement("PrivateKey");
    if(!key) {
        logger::log_message("FATAL", "Server", std::format("private key not found: exiting pid={}", getpid()));
        EXIT_FATAL("loading server configuration", 0, "Private Key Not Found", "please provide a valid key");
    }
    ssl.key_path = key->GetText();
    port = port == 80 ? 443 : port;
}

void cfg::Config::loadHostIP() {
    asio::io_context io;
    asio::ip::tcp::resolver resolver(io);
    asio::ip::tcp::resolver::query query(asio::ip::host_name(), "");
    asio::ip::tcp::resolver::iterator endpoints = resolver.resolve(query);

    for (auto it = endpoints; it != asio::ip::tcp::resolver::iterator(); ++it) {
        auto endpoint = it->endpoint();
        if (endpoint.address().is_v4()) { 
            host_address = endpoint.address().to_string();
            return;
        }
    }
    host_address = "127.0.0.1";
}
