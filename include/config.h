#ifndef CONFIG_H
#define CONFIG_H

#include <tinyxml2.h>
#include <string>
#include <unordered_map>

#include "logger.h"

namespace config {

using Endpoint = std::string;

struct Route {
    std::string method{""};
    Endpoint endpoint{""};
    std::string script{""};
    bool is_protected{false};
    std::string role{"viewer"};
    bool is_authenticator{false};
    Route() {}
};

struct ServerConfig {
    std::string content_path;
    std::unordered_map<Endpoint, Route> routes;
};

const Route* find_route(const std::unordered_map<Endpoint, Route>& routes, const Endpoint& endpoint);
void print_routes(const std::unordered_map<Endpoint, Route>& routes);
bool load_config(const std::string& config_path, ServerConfig& server_config);

};
#endif