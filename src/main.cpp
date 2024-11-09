#include "Server.h"

#define CONFIG_PATH "/home/isaac/Web-Server/example/server.config"
#define PORT 1025

int main(int argc, char** argv)
{

    config::ServerConfig config;
    if(!config::load_config(CONFIG_PATH, config)) {
        return 0;
    }
    
    LOG("INFO", "configuration loaded, serving from", "%s", config.content_path.c_str());
    if(chroot(config.content_path.c_str()) < 0)
    {
        std::cout << "chroot to " << config.content_path << " failed, shutting down...\n";
        return 0;
    }

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