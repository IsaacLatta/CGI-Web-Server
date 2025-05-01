#include "config.h"
#include "Router.h"
#include "Middleware.h"
#include "Transaction.h"

using namespace cfg;

Config Config::INSTANCE;
std::once_flag Config::initFlag;
mw::Pipeline PIPELINE;

std::string cfg::DEFAULT_MAKE_KEY(Transaction* txn) {
    return txn->getSocket()->getIP();
};


Config::Config() {}

const Config* Config::getInstance(const std::string& config_path) {
    if(!config_path.empty()) {
        std::call_once(Config::initFlag, [config_path]() {Config::INSTANCE.initialize(config_path);});
    }
    return &Config::INSTANCE;
}

static void print_endpoint(const http::EndpointMethod& method, const std::string& endpoint_url) {
    std::string msg = std::format(
        "route [{} {}]\n\taccess_role={}\n\tauth_role={}\n\tprotected={}\n\tauthenticator={}\n\tscript={}\n\targs={}\n",
        http::method_enum_to_str(method.m),
        endpoint_url,
        method.access_role,
        method.auth_role,
        method.is_protected ? "true" : "false",
        method.is_authenticator ? "true" : "false",
        method.resource.empty() ? "none" : method.resource,
        static_cast<int>(method.args)
    );
    TRACE("Server", "%s", msg.c_str());
}

static int get_seconds_multiplier(const std::string& unit) {
    if (unit == "d" || unit == "day" || unit == "days") {
        return 24*3600;
    } else if (unit == "h" || unit == "hr" || unit == "hour" || unit == "hours") {
        return 3600;
    } else if (unit == "m"   || unit == "min" || unit == "mins") {
        return 60;
    } else if (unit == "s" || unit == "sec" || unit == "secs") {
        return 1;
    } else {
        WARN("Server", "unknown time unit '%s' for rate limit, assuming seconds", unit.c_str());
        return 1;
    }
}

static int get_seconds_from_time_str(const char* data) {
    if(!data) {
        DEBUG("Server", "empty time field, resulting to default %s seconds", cfg::DEFAULT_WINDOW_SECONDS);
        return cfg::DEFAULT_WINDOW_SECONDS;
    }
    
    std::string str(data);
    std::string token;
    std::istringstream iss(str);

    if (!(iss >> token)) {
        WARN("Server", "missing time value for rate limit, defaulting to %d seconds",
             cfg::DEFAULT_WINDOW_SECONDS);
        return cfg::DEFAULT_WINDOW_SECONDS;
    }
    std::size_t pos = 0;
    while (pos < token.size() && std::isdigit(static_cast<unsigned char>(token[pos]))) {
        ++pos; // move to the beginning of the unit
    }
    if (pos == 0) {
        WARN("Server", "invalid time value '%s' for rate limit, defaulting to %d seconds",
             token.c_str(), cfg::DEFAULT_WINDOW_SECONDS);
        return cfg::DEFAULT_WINDOW_SECONDS;
    }
    int value = 0;
    try {
        value = std::stoi(token.substr(0, pos));
    }
    catch (const std::exception&) {
        WARN("Server", "couldn't parse numeric part of '%s' for rate limit, defaulting to %d seconds",
             token.c_str(), cfg::DEFAULT_WINDOW_SECONDS);
        return cfg::DEFAULT_WINDOW_SECONDS;
    }
    std::string unit = token.substr(pos);
    std::transform(unit.begin(), unit.end(), unit.begin(), [](unsigned char c){ return std::tolower(c); });

    return get_seconds_multiplier(unit)*value;
}

static int load_int(const char* int_str, int fallback, const std::string& fallback_log) {
    if(!int_str) {
        DEBUG("Server", "%s", fallback_log.c_str());
        return fallback;
    }
    try {
        return std::stoi(int_str);
    } catch (const std::exception& e) {
        DEBUG("Server", "%s", fallback_log.c_str());
        return fallback;
    }
}

static int load_refill_rate(const char* buf) {
    if(!buf) {
        DEBUG("Server", "failed to parse refill_rate, defaulting to %d tokens/second", cfg::DEFAULT_REFILL_RATE);
        return cfg::DEFAULT_REFILL_RATE;
    }
    std::string rate_str(buf);
    std::size_t pos(0);
    while(pos < rate_str.size() && std::isdigit(static_cast<unsigned char>(rate_str[pos]))) {
        ++pos;
    }
    if(pos == 0) {
        DEBUG("Server", "failed to parse refill_rate, defaulting to %d tokens/second", cfg::DEFAULT_REFILL_RATE);
        return cfg::DEFAULT_REFILL_RATE;
    }
    std::string digit_str = rate_str.substr(0, pos);
    int value(0);
    try {
        value = std::stoi(digit_str);
    } catch (const std::exception& e) {
        WARN("Server", "couldn't parse numeric part of '%s' for rate limit, defaulting to %d tokens/second",
             rate_str.c_str(), cfg::DEFAULT_REFILL_RATE);
    }
    if((pos = rate_str.find("/")) == std::string::npos) {
        WARN("Server", "couldn't parse refill_rate missing '/' for tokens/time, defaulting to %d tokens/second",
             rate_str.c_str(), cfg::DEFAULT_REFILL_RATE);
    }
    return value / get_seconds_multiplier(rate_str.substr(pos + 1));
}

static std::unique_ptr<mw::Middleware> load_token_bucket(tinyxml2::XMLElement* algo_elem, bool* uses_ip) {
    cfg::TokenBucketSetting setting;
    setting.capacity = load_int(algo_elem->Attribute("capacity"), cfg::DEFAULT_TOKEN_CAPACITY, 
        std::format("failed to parse capacity for rate limit, defaulting to capacity=%d tokens", cfg::DEFAULT_TOKEN_CAPACITY));
    setting.refill_rate = load_refill_rate(algo_elem->Attribute("refill_rate"));
    DEBUG("Server", "RateLimit [algorithm='token bucket' capacity=%d refill_rate=%d tokens/second]", setting.capacity, setting.refill_rate);
    return std::make_unique<mw::TokenBucketLimiter>(std::move(setting));
}

static std::unique_ptr<mw::Middleware> load_fixed_window(tinyxml2::XMLElement* algo_elem, bool* uses_ip = nullptr) {
    cfg::FixedWindowSetting setting;
    setting.max_requests = load_int(algo_elem->Attribute("max_requests"), cfg::DEFAULT_MAX_REQUESTS, 
        std::format("failed to parse max_requests for rate limit, defaulting to {} requests", cfg::DEFAULT_MAX_REQUESTS));
    setting.window_seconds = algo_elem->Attribute("window") ? get_seconds_from_time_str(algo_elem->Attribute("window")) : cfg::DEFAULT_WINDOW_SECONDS;

    DEBUG("Server", "RateLimit [algorithm='fixed window' max_requests=%d window=%ds] loaded", setting.max_requests, setting.window_seconds);
    return std::make_unique<mw::FixedWindowLimiter>(setting);
}

static std::unique_ptr<mw::Middleware> load_limiter(tinyxml2::XMLElement* algo_elem, bool* uses_ip = nullptr) {
    if(!algo_elem) {
        WARN("Server", "parsing error with RateLimit, default RateLimit [algo='fixed window' max_requests=%d window=%ds] loaded", 
        cfg::DEFAULT_MAX_REQUESTS, cfg::DEFAULT_WINDOW_SECONDS);
        return std::make_unique<mw::FixedWindowLimiter>(FixedWindowSetting());
    }
    
    std::string algo = algo_elem->Attribute("algorithm") ? algo_elem->Attribute("algorithm") : "";
    if(algo.empty()) {
        WARN("Server", "rate limiting algorithm is empty, defaulting to algorithm='fixed window'");
        return load_fixed_window(algo_elem, uses_ip);
    }  else if (algo == "fixed window" || algo == "fixed_window") {
        return load_fixed_window(algo_elem, uses_ip);
    } else if (algo == "token bucket" || algo == "token_bucket") {
        return load_token_bucket(algo_elem, uses_ip);
    } else {
        WARN("Server", "rate limiting algo=%s not supported, default RateLimit [algorithm='fixed window' max_requests=%d window=%ds] loaded", 
        cfg::DEFAULT_MAX_REQUESTS, cfg::DEFAULT_WINDOW_SECONDS);
        return std::make_unique<mw::FixedWindowLimiter>(FixedWindowSetting());
    }
}

std::unique_ptr<mw::Middleware> Config::loadGlobalRateLimit(tinyxml2::XMLDocument* doc, bool* uses_ip = nullptr) {
    auto rate_limit_elem = doc->FirstChildElement("ServerConfig")->FirstChildElement("RateLimit");
    if(!rate_limit_elem) {
        DEBUG("Server", "no global rate limit configuration: default RateLimit [algorithm='fixed window' max_requests=%d window=%ds] loaded", cfg::DEFAULT_MAX_REQUESTS, cfg::DEFAULT_WINDOW_SECONDS);
        return std::make_unique<mw::FixedWindowLimiter>();
    }

    auto global_elem = rate_limit_elem->FirstChildElement("Global");
    if(!global_elem) {
        DEBUG("Server", "no global rate limit configuration: default RateLimit [algorithm='fixed window' max_requests=%d window=%ds] loaded", cfg::DEFAULT_MAX_REQUESTS, cfg::DEFAULT_WINDOW_SECONDS);
        return std::make_unique<mw::FixedWindowLimiter>();
    } else if(global_elem->Attribute("disable") && !std::strcmp(global_elem->Attribute("disable"), "true")) {
        DEBUG("Server", "global rate limit disabled");
        return nullptr;
    } else {
        TRACE("Server", "loading global rate limiter ...");
        return load_limiter(global_elem->FirstChildElement("Algorithm"), uses_ip);
    }
}

void Config::loadPipeline(tinyxml2::XMLDocument* doc) {
    bool global_uses_ip = true;
    auto limiter = loadGlobalRateLimit(doc, &global_uses_ip);
    PIPELINE.components.push_back(std::make_unique<mw::Logger>());
    PIPELINE.components.push_back(std::make_unique<mw::ErrorHandler>());
    if(limiter && global_uses_ip) {
        PIPELINE.components.push_back(std::move(limiter));
    }
    PIPELINE.components.push_back(std::make_unique<mw::Parser>());
    if(limiter && !global_uses_ip) {
        PIPELINE.components.push_back(std::move(limiter));
    }
    PIPELINE.components.push_back(std::make_unique<mw::RateLimiter>());
    PIPELINE.components.push_back(std::make_unique<mw::Authenticator>());
    TRACE("Server", "pipeline loaded");
}

mw::Pipeline* Config::getPipeline() const {
    return &PIPELINE;
}

void Config::loadRoutes(tinyxml2::XMLDocument* doc, const std::string& content_path) {
    using namespace tinyxml2;
    using namespace cfg;
    
    http::Router* router = &http::Router::INSTANCE;

    XMLElement* doc_routes = doc->FirstChildElement("ServerConfig")->FirstChildElement("Routes");
    if(!doc_routes) {
        return;
    }

    XMLElement* route_el = doc_routes->FirstChildElement("Route");
    while(route_el) {
        http::EndpointMethod method;

        std::string endpoint_url = route_el->Attribute("endpoint") ? route_el->Attribute("endpoint") : "";
        if(endpoint_url.empty()) {
            WARN("Server", "endpoint url is empty, defaulting to root \"/\"");
            endpoint_url = "/";  // Set default to root
        }

        std::string method_str = route_el->Attribute("method") ? route_el->Attribute("method") : "";
        method.m = http::method_str_to_enum(method_str);
        
        method.resource = route_el->Attribute("script") ? content_path + "/" + route_el->Attribute("script") : "";
        method.has_script = !method.resource.empty();
        method.is_protected = route_el->Attribute("protected") && std::string(route_el->Attribute("protected")) == "true";
        if(method.is_protected) {
            method.access_role = route_el->Attribute("access_role") ? route_el->Attribute("access_role") : "";
            if (method.access_role.empty()) {
                WARN("Server", "protected route[%s %s] is missing access role attribute, defaulting to admin", 
                    method_str.c_str(), endpoint_url.c_str());
                method.access_role = ADMIN_ROLE_HASH;
            }
        }
        else {
            method.access_role = VIEWER_ROLE_HASH;
        }

        method.is_authenticator = route_el->Attribute("authenticator") && std::string(route_el->Attribute("authenticator")) == "true";
        if(method.is_authenticator) {
            method.auth_role = route_el->Attribute("auth_role") ? route_el->Attribute("auth_role") : "";
            if(method.auth_role.empty()) {
                WARN("Server", "route [%s %s] is missing auth role, defaulting to lowest privilege viewer", method_str.c_str(), endpoint_url.c_str());
                method.auth_role = cfg::VIEWER_ROLE_HASH;
            }
        }
        method.args = route_el->Attribute("args") ? http::arg_str_to_enum(route_el->Attribute("args")) : http::arg_type::None;
        if (method.m != http::method::Not_Allowed && !endpoint_url.empty()) {
                tinyxml2::XMLElement* rate_limit_el = route_el->FirstChildElement("RateLimit");
                if(rate_limit_el) {
                    TRACE("Server", "loading rate limiter for [%s %s] ...", method_str.c_str(), endpoint_url.c_str());
                    method.rate_limiter = std::shared_ptr<mw::Middleware>(load_limiter(rate_limit_el));
                }
                print_endpoint(method, endpoint_url);
                router->updateEndpoint(endpoint_url, std::move(method));
            } 
        else {
            ERROR("Server", "incomplete route attribute: parsed method[%s] and endpoint[%s], both required", method_str.c_str(), endpoint_url.c_str());
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

void display_role(cfg::Role* role) {
    std::string includes;
    for (const auto& include : role->includes) {
        includes += "include=" + include + "\n\t";
    }
    if (!includes.empty()) {
        includes.pop_back(); 
        includes.pop_back(); 
    }
    std::string final_msg = std::format("role [{}]\n\t{}\n", role->title, includes);
    TRACE("Server", "%s", final_msg.c_str());
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
            FATAL("Server", "xml error: role is missing title");
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
        FATAL("Server", "failed to open jwt secret file[%s], errno=%d %s", file_path.c_str(), errno, strerror(errno));
    }
    
    struct stat file_stat;
    if(fstat(filefd, &file_stat) < 0) {
        close(filefd);
        FATAL("Server", "failed to stat jwt secret file[%s], errno=%d %s", file_path.c_str(), errno, strerror(errno));
    }
    this->secret.resize(file_stat.st_size);

    ssize_t bytes_read = 0, bytes;
    while(bytes_read < file_stat.st_size) {
        if((bytes = read(filefd, this->secret.data() + bytes_read, file_stat.st_size - bytes_read))  < 0) {
            close(filefd);
            FATAL("Server", "failed to read jwt secret file[%s], errno=%d %s", file_path.c_str(), errno, strerror(errno));
        }
        if(bytes == 0) {
            break;
        }
        bytes_read += bytes;
    }
    close(filefd);

    if(bytes_read != file_stat.st_size) {
        FATAL("Server", "jwt secret file[%s] read incomplete, %zd/%ld bytes read", file_path.c_str(), bytes_read, file_stat.st_size);
    }

    if(this->secret.empty() || file_stat.st_size == 0) {
        FATAL("Server", "invalid jwt secret file[%s], file may be empty", file_path.c_str());
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
        FATAL("Server", "failed to generate jwt secret");
    }
    return convert_to_hex(buffer.data(), buffer.size());
}

void Config::generateJWTSecret(tinyxml2::XMLElement* secret_elem) {
    std::size_t secret_len = cfg::DEFAULT_JWT_SECRET_SIZE;
    if(!secret_elem->Attribute("enable") || std::strcmp(secret_elem->Attribute("enable"), "true")) {
        WARN("Server", "ignoring JWT configuration: generate jwt secret is disabled");
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
            FATAL("Server", "loading configuration: jwt secret string is invalid");
        } 
        return;
    }

    if((secret_elem = jwt_elem->FirstChildElement("GenerateSecret"))) {
        generateJWTSecret(secret_elem);
        return;
    }
    
    WARN("Server", "ignoring jwt configuration: no options set");
}

static void print_error_page(const http::ErrorPage& error_page, const std::string& file) {
    std::string msg = "Error Page [" + std::to_string(static_cast<int>(error_page.status)) + " " + file + "] loaded";
    TRACE("Server", "%s", msg.c_str());
}

void Config::loadErrorPages(tinyxml2::XMLDocument* doc) {
    tinyxml2::XMLElement* error_pg_elem = doc->FirstChildElement("ServerConfig")->FirstChildElement("ErrorPages");
    if(!error_pg_elem) {
        return;
    }

    http::Router* router = &http::Router::INSTANCE;
    tinyxml2::XMLElement* error_pg = error_pg_elem->FirstChildElement("ErrorPage");
    while(error_pg) {
        http::ErrorPage error_page;
        error_page.status = error_pg->Attribute("code") ? http::code_str_to_enum(error_pg->Attribute("code")) : http::code::Not_A_Status;
        std::string file = error_pg->Attribute("file") ? error_pg->Attribute("file") : "";
        if(error_page.status == http::code::Not_A_Status || file.empty()) {
            WARN("Server", "loading configuration: invalid error page, code=%d, file=%s", static_cast<int>(error_page.status), file.c_str());
        } else {
            print_error_page(error_page, file);
            router->addErrorPage(std::move(error_page), std::move(file));
        }
        error_pg = error_pg_elem->NextSiblingElement("ErrorPage");
    }
}

void Config::loadThreads(tinyxml2::XMLDocument* doc) {
    std::size_t fallback = 4;
    auto* threads_el = doc->FirstChildElement("ServerConfig")->FirstChildElement("Threads");
    if(!threads_el) {
        thread_count = std::thread::hardware_concurrency();
        if (thread_count == 0) {
            WARN("Server", "unable to detect CPU cores, defaulting to %zu", fallback);
            thread_count = fallback;
        }
        DEBUG("Server", "allocating %zu threads", thread_count);
        return;
    }
    
    const char* txt = threads_el->GetText();
    if (txt && *txt) {
        std::size_t tmp = 0;
        auto [ptr, ec] = std::from_chars(txt, txt + std::strlen(txt), tmp);
        if (ec == std::errc() && tmp > 0) {
            thread_count = tmp;
        } else {
            WARN("Server", "invalid threads '%s', defaulting to %zu", txt, fallback);
            thread_count = fallback;
        }
    } else {
        WARN("Server", "threads element empty, defaulting to %zu", fallback);
        thread_count = fallback;
    }
    DEBUG("Server", "allocating %zu threads", thread_count);
}

static std::string resolve_log_path(const std::string& path) {
    std::string log_dir = "log";
    char resolved[PATH_MAX];
    if(realpath(path.c_str(), resolved)) {
        log_dir = resolved;
    }
    std::filesystem::create_directories(log_dir);
    return log_dir;
}

void Config::initialize(const std::string& config_path) {
    tinyxml2::XMLDocument doc;
    
    auto logger = logger::Logger::getInstance();
    logger->addSink(std::move(std::make_unique<logger::ConsoleSink>()));
    logger->start();
    TRACE("Server", "parsing config file: %s", config_path.c_str());

    if(doc.LoadFile(config_path.c_str()) != tinyxml2::XML_SUCCESS) {
        FATAL("Server", "loading configuration failed [error=%d %s]", static_cast<int>(doc.ErrorID()), doc.ErrorStr());
    }

    if(!doc.FirstChildElement("ServerConfig")) {
        FATAL("Server", "loading configuration: no ServerConfig element, must provide <ServerConfig></ServerConfig> to load configuration from");
    }

    tinyxml2::XMLElement* web_dir = doc.FirstChildElement("ServerConfig")->FirstChildElement("WebDirectory");
    if(web_dir && web_dir->GetText()) {
        this->content_path = web_dir->GetText();
    }
    else {
        FATAL("Server", "loading configuration failed: no web directory to serve from");
    }

    DEBUG("Server", "Web Directory found: %s", content_path.c_str());

    if(chdir(content_path.c_str()) < 0) {
        FATAL("Server", "failed to chdir to web directory(%s): [error=%d %s]", content_path.c_str(), errno, strerror(errno));
    }

    TRACE("Server", "changed directory to: %s", content_path.c_str());

    tinyxml2::XMLElement* host = doc.FirstChildElement("ServerConfig")->FirstChildElement("Host");
    if(host) {
        host_name = host->GetText();
    }
    else {
        host_name = cfg::NO_HOST_NAME;
    }

    tinyxml2::XMLElement* log_dir = doc.FirstChildElement("ServerConfig")->FirstChildElement("LogDirectory");
    if(log_dir && log_dir->GetText()) {
        this->log_path = resolve_log_path(log_dir->GetText());
    } 
    else {
        this->log_path = resolve_log_path("./log");
    }
    logger->addSink(std::move(std::make_unique<logger::FileSink>(log_path, host_name)));

    loadJWTSecret(&doc);

    tinyxml2::XMLElement* host_port = doc.FirstChildElement("ServerConfig")->FirstChildElement("Port");
    port = host_port ? std::stoi(host_port->GetText()) : 80;

    loadThreads(&doc);
    loadErrorPages(&doc);
    loadRoles(&doc);
    loadSSL(&doc);
    loadRoutes(&doc, content_path);
    loadHostIP();
    loadPipeline(&doc);
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
        FATAL("Server", "loading configuration: ssl certificate not found");
    }
    ssl.certificate_path = cert->GetText();
    tinyxml2::XMLElement* key = ssl_config->FirstChildElement("PrivateKey");
    if(!key) {
        FATAL("Server", "loading configuration: private key not found");
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
