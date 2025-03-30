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
            logger::log_message(logger::FATAL, "Server", std::format("parsed role missing title, exiting pid={}", getpid()));
            EXIT_FATAL("FATAL", 0, "XML Error", "role missing title, exiting pid=%d", getpid());
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

void Config::loadJWTSecretFromFile(tinyxml2::XMLElement* secret_elem) {
    std::string file_path = secret_elem->GetText() == nullptr ? "" : secret_elem->GetText(); // Let errno handle it
    int filefd = open(file_path.c_str(), O_RDONLY);
    if(filefd < 0) {
        logger::log_message(logger::FATAL, "Server", std::format("{} ({}): exiting pid={}", strerror(errno), errno, getpid()));
        EXIT_FATAL("loading server configuration", errno, strerror(errno), "failed to open secret file: %s", file_path.c_str());
    }
    
    struct stat file_stat;
    if(fstat(filefd, &file_stat) < 0) {
        close(filefd);
        logger::log_message(logger::FATAL, "Server", std::format("{} ({}): exiting pid={}", strerror(errno), errno, getpid()));
        EXIT_FATAL("loading server configuration", errno, strerror(errno), "failed to stat secret file: %s", file_path.c_str());
    }
    this->secret.resize(file_stat.st_size);

    ssize_t bytes_read = 0, bytes;
    while(bytes_read < file_stat.st_size) {
        if((bytes = read(filefd, this->secret.data() + bytes_read, file_stat.st_size - bytes_read))  < 0) {
            close(filefd);
            logger::log_message(logger::FATAL, "Server", std::format("{} ({}): exiting pid={}", strerror(errno), errno, getpid()));
            EXIT_FATAL("loading server configuration", errno, strerror(errno), "failed to read secret file: %s", file_path.c_str());
        }
        if(bytes == 0) {
            break;
        }
        bytes_read += bytes;
    }
    close(filefd);

    if(bytes_read != file_stat.st_size) {
        logger::log_message(logger::FATAL, "Server", std::format("{} ({}): exiting pid={}", strerror(errno), errno, getpid()));
        EXIT_FATAL("loading server configuration", 0, "secret file read incomplete", "read %zd of %ld bytes", bytes_read, file_stat.st_size);  
    }

    if(this->secret.empty() || file_stat.st_size == 0) {
        logger::log_message(logger::FATAL, "Server", std::format("{} ({}): exiting pid={}", strerror(errno), errno, getpid()));
        EXIT_FATAL("loading server configuration", 0, "secret file is empty", "must provide a valid secret for JWT");  
    }
}

static std::string convert_to_hex(const unsigned char* buffer, std::size_t buf_len) {
    std::ostringstream oss;
    for(std::size_t i = 0; i < buf_len; ++i) {
        oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(buffer[i]);
    }
    return oss.str();
}

static std::string generate_rand_secret(std::size_t len) {
    std::vector<unsigned char> buffer(len);
    if(RAND_bytes(buffer.data(), static_cast<int>(len)) != 1) {
        logger::log_message(logger::FATAL, "Server", "failed to generate JWT secret");
        EXIT_FATAL("loading server configuration", 0, "random generation error", "failed to generate JWT secret");
    } 
    return convert_to_hex(buffer.data(), buffer.size());
}

void Config::generateJWTSecret(tinyxml2::XMLElement* secret_elem) {
    std::size_t secret_len = cfg::DEFAULT_JWT_SECRET_SIZE;
    if(!secret_elem->Attribute("enable") || std::strcmp(secret_elem->Attribute("enable"), "true")) {
        logger::log_message(logger::WARN, "Server", "ignoring JWT configuration: generate jwt secret is disabled");
        return;
    }

    tinyxml2::XMLElement* len_elem;
    if((len_elem = secret_elem->FirstChildElement("Length")) && len_elem->GetText()) {
        secret_len = std::stoi(len_elem->GetText()); 
    }

    this->secret = generate_rand_secret(secret_len);
}

void Config::loadJWTSecret(tinyxml2::XMLDocument* doc) {
    tinyxml2::XMLElement* jwt_elem = doc->FirstChildElement("ServerConfig")->FirstChildElement("JWT");
    if(!jwt_elem) {
        return;
    }
    
    tinyxml2::XMLElement* secret_elem;
    if((secret_elem = jwt_elem->FirstChildElement("SecretFile"))) { 
        loadJWTSecretFromFile(secret_elem); 
        return;
    }

    if((secret_elem = jwt_elem->FirstChildElement("Secret"))) {
        this->secret = secret_elem->GetText() == nullptr ? "" : secret_elem->GetText();
        if(this->secret.empty()) {
            logger::log_message(logger::FATAL, "Server", std::format("{}: exiting pid={}", "secret string is empty", getpid()));
            EXIT_FATAL("loading server configuration", 0, "secret string is empty", "must provide a valid secret for JWT");  
        } 
        return;
    }

    if((secret_elem = jwt_elem->FirstChildElement("GenerateSecret"))) {
        generateJWTSecret(secret_elem);
        return;
    }
    
    logger::log_message(logger::WARN, "Server", "ignoring JWT configuration: no options set");
}

void Config::initialize(const std::string& config_path) {
    tinyxml2::XMLDocument doc;
    
    if(doc.LoadFile(config_path.c_str()) != tinyxml2::XML_SUCCESS) {
        logger::log_message("FATAL", "Server", std::format("Configuration Failed [error={} {}]", static_cast<int>(doc.ErrorID()), doc.ErrorStr()));
        EXIT_FATAL("loading server configuration", static_cast<int>(doc.ErrorID()), doc.ErrorStr(), "failed to load config file: %s", config_path.c_str());
    }

    tinyxml2::XMLElement* web_dir = doc.FirstChildElement("ServerConfig")->FirstChildElement("WebDirectory");
    if(web_dir && web_dir->GetText()) {
        this->content_path = web_dir->GetText();
    }
    else {
        logger::log_message(logger::FATAL, "Server", std::format("Configuration Failed [error={} {}]", static_cast<int>(doc.ErrorID()), doc.ErrorStr()));
        EXIT_FATAL("loading server configuration", 0, "no web directory path", "please provide a path to serve from");
    }

    if(chdir(content_path.c_str()) < 0) {
        logger::log_message(logger::FATAL, "Server", std::format("Configuration Failed [error={} {}]", errno, strerror(errno)));
        EXIT_FATAL("loading server configuration", errno, strerror(errno), "failed to chdir to web directory path: %s", content_path.c_str());
    }

    tinyxml2::XMLElement* host = doc.FirstChildElement("ServerConfig")->FirstChildElement("Host");
    if(host) {
        host_name = host->GetText();
    }
    else {
        host_name = cfg::NO_HOST_NAME;
    }

    tinyxml2::XMLElement* log_dir = doc.FirstChildElement("ServerConfig")->FirstChildElement("LogDirectory");
    if(log_dir && log_dir->GetText()) {
        this->log_path = log_dir->GetText();
    }

    logger::log_message(logger::STATUS, "Server", std::format("{} initialiizing ...", this->host_name));

    loadJWTSecret(&doc);

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
        logger::log_message(logger::FATAL, "Server", std::format("certificate not found: exiting pid={}", getpid()));
        EXIT_FATAL("loading server configuration", 0, "Certificate Not Found", "please provide a valid certificate");
    }
    ssl.certificate_path = cert->GetText();
    tinyxml2::XMLElement* key = ssl_config->FirstChildElement("PrivateKey");
    if(!key) {
        logger::log_message(logger::FATAL, "Server", std::format("private key not found: exiting pid={}", getpid()));
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
