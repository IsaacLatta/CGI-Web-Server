#ifndef CONFIG_H
#define CONFIG_H

#include <asio.hpp>
#include <tinyxml2.h>
#include <string>
#include <unordered_map>
#include <mutex>

#include "logger.h"

namespace cfg {

constexpr std::string VIEWER = "viewer";
constexpr std::string USER = "user";
constexpr std::string ADMIN = "admin";
constexpr std::string NO_HOST_NAME  = "";

using Endpoint = std::string;

struct SSLConfig {
    bool active;
    std::string key_path;
    std::string certificate_path;
};

struct Route {
    std::string method{""};
    Endpoint endpoint{""};
    std::string script{""};
    bool is_protected{false};
    std::string role{VIEWER};
    bool is_authenticator{false};
    Route() {}
};

using Routes = std::unordered_map<Endpoint, Route>;

class Config 
{
    public:
    static const Config* getInstance(const std::string& config_path = "");
    void initialize(const std::string& config_path);

    const Route* findRoute(const Endpoint& endpoint) const;
    const Routes* getRoutes() const {return &routes;}
    const std::string& getContentPath() const {return content_path;}
    const std::string getSecret() const {return secret;}
    const std::string getServerName() const {return host_name;}
    const SSLConfig* getSSL() const {return &ssl;}
    std::string getHostIP() const {return host_address;}
    int getPort() const {return port;}
    void printRoutes() const;

    private:
    Config();

    Config(Config&) = delete;
    void operator=(Config&) = delete;

    void loadSSL(tinyxml2::XMLDocument* doc);
    void loadHostIP();
    void loadRoutes(tinyxml2::XMLDocument* doc, const std::string& content_path);

    private:
    static Config INSTANCE;
    static std::once_flag initFlag;
    
    Routes routes;
    SSLConfig ssl;
    std::string secret = "top-secret";
    std::string content_path;
    std::string host_name;
    std::string host_address;
    int port;
};

std::string getRoleHash(const std::string& role);

};
#endif