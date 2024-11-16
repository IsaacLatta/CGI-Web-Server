#include "Server.h"

#define CONFIG_PATH "/home/isaac/Web-Server/example/server.config"
#define PORT 1025

int main(int argc, char** argv)
{
    const cfg::Config* config = cfg::Config::getInstance(CONFIG_PATH);
    
    LOG("INFO", "configuration loaded, serving from", "%s", config->getContentPath().c_str());
    
    if(chdir(config->getContentPath().c_str()) < 0)
    {
        std::cout << "chdir to " << config->getContentPath().c_str() << " failed, shutting down...\n";
        return 0;
    }
    logger::log_message("STATUS", "Server", std::format("Configuration loaded from file: {}", CONFIG_PATH));
    config->printRoutes();

    try
    {
        Server web(config, PORT);
        web.start();
    }
    catch(const std::exception& e)
    {
        std::cerr << e.what() << '\n';
    }
   
    return 0;
}