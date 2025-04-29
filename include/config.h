#ifndef CONFIG_H
#define CONFIG_H

#include <asio.hpp>
#include <tinyxml2.h>
#include <string>
#include <unordered_map>
#include <mutex>
#include <format>
#include <openssl/rand.h>

#include "logger_macros.h"
#include "http.h"

namespace mw {
    class Middleware;
    struct Pipeline;
}

namespace cfg {

using Endpoint = std::string;

struct Role {
    std::string title;
    std::vector<std::string> includes;

    bool includesRole(const std::string& role) const {
        return (title == role) || (std::find(includes.begin(), includes.end(), role) != includes.end());
    }
};

struct RateSetting {
    int window_seconds{60};
    int max_requests{5000};  
};

std::string get_role_hash(std::string role_title);

const std::string VIEWER_ROLE_HASH = get_role_hash("viewer");
const std::string USER_ROLE_HASH = get_role_hash("user");
const std::string ADMIN_ROLE_HASH = get_role_hash("admin");
const Role VIEWER = {VIEWER_ROLE_HASH, {}};
const Role USER = {USER_ROLE_HASH, {VIEWER_ROLE_HASH}};
const Role ADMIN = {ADMIN_ROLE_HASH, {USER_ROLE_HASH, VIEWER_ROLE_HASH}};

const std::string NO_HOST_NAME  = "server";
const size_t DEFAULT_JWT_SECRET_SIZE = 64;

struct SSLConfig {
    bool active;
    std::string key_path;
    std::string certificate_path;
};

using Roles = std::unordered_map<std::string, Role>;

class Config 
{
    public:
    static const Config* getInstance(const std::string& config_path = "");
    void initialize(const std::string& config_path);

    mw::Pipeline* getPipeline() const;
    const Role* findRole(const std::string& role_title) const;
    const std::string& getContentPath() const {return content_path;}
    const std::string getLogPath() const {return log_path;}
    const std::string getSecret() const {return secret;}
    const std::string getServerName() const {return host_name;}
    const SSLConfig* getSSL() const {return &ssl;}
    std::string getHostIP() const {return host_address;}
    int getPort() const {return port;}
    std::size_t getThreadCount() const {return thread_count;}

    private:
    Config();

    Config(Config&) = delete;
    void operator=(Config&) = delete;

    void loadRoles(tinyxml2::XMLDocument* doc);
    void loadSSL(tinyxml2::XMLDocument* doc);
    void loadHostIP();
    void loadRoutes(tinyxml2::XMLDocument* doc, const std::string& content_path);
    void loadJWTSecret(tinyxml2::XMLDocument* doc);
    void loadJWTSecretFromFile(tinyxml2::XMLElement* secret_elem);
    void generateJWTSecret(tinyxml2::XMLElement* secret_elem);
    void loadThreads(tinyxml2::XMLDocument* doc);
    void loadErrorPages(tinyxml2::XMLDocument* doc);
    void loadPipeline(tinyxml2::XMLDocument* doc);

    private:
    std::size_t thread_count{0};
    static Config INSTANCE;
    static std::once_flag initFlag;
    
    Roles roles;
    // Routes routes;
    SSLConfig ssl;
    std::string secret;
    std::string content_path;
    std::string host_name;
    std::string host_address;
    std::string log_path = "";
    int port;
};


};
#endif