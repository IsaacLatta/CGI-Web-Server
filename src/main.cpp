#include "Server.h"

#define CONFIG_PATH "/home/isaac/Projects/Web-Server/example/server.config"

int main(int argc, char** argv)
{
    const cfg::Config* config = cfg::Config::getInstance(CONFIG_PATH);
    
    LOG("INFO", "configuration loaded, serving from", "%s", config->getContentPath().c_str());
    logger::log_message("STATUS", "Server", std::format("configuration loaded from file: {}", CONFIG_PATH));
    config->printRoutes();

    try {
        Server server(config);
        server.start();
    }
    catch(const std::exception& e) {
        std::cerr << e.what() << '\n';
    }
   
    return 0;
}