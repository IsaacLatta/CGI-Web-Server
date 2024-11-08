#ifndef CONFIG_H
#define CONFIG_H

#include <tinyxml2.h>
#include <string>
#include <unordered_map>

#include "logger.h"

namespace config {

struct RouteConfig {
    std::string method;
    std::string endpoint;
    std::string script;
    bool is_protected;
};

struct ServerConfig {
    std::string content_path;
    std::unordered_map<std::string, RouteConfig> routes;
};

bool load_config(const std::string& config_path, ServerConfig& server_config);

};
#endif