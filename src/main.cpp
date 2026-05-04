#include "Server.h"

#include <openssl/conf.h>

int main(int argc, char** argv)
{
    OPENSSL_no_config();

    if(argc < 2 && argv[1] == nullptr) {
        std::cerr << "ERROR no configuration path, must provide a path to load configuration from\n";
        return 0;
    }

    const cfg::Config* config = cfg::Config::getInstance(argv[1]);
    STATUS("Server", "configuration loaded from file: %s", argv[1]);
    STATUS("Server", "serving from: %s", config->getContentPath().c_str());
    STATUS("Server", "writing logs to: %s", config->getLogPath().c_str());

    try {
        Server server(config);
        server.start();
    }
    catch(const std::exception& e) {
        std::cerr << "[ERROR] " << e.what() << std::endl;
    }
   
    return 0;
}