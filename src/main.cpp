#include "Server.h"

/* TO DO
*
* [1] POST Request
*   - Add support for resource uploads, as well as task submission, e.g.) streaming
* [2] PUT Request
*   - Allow upload of resource
* [3] OPTIONS Request
*   - Allow preflight for resoruce creation (use with POST)
* [4] CONNECT Support
*   - For tunneling (HTTPS)
* [5] Default endpoints
*   - Endpoints for health or status metrics, e.g.) /health, /status
* [6] Add rate limiting, especially for POST
*
*/

int main(int argc, char** argv)
{
    if(argc < 2 && argv[1] == nullptr) {
        // LOG("ERROR", "no configuration path", "must provide a path to load configuration from.");
        return 0;
    }

    const cfg::Config* config = cfg::Config::getInstance(argv[1]);
    
    // LOG("INFO", "configuration loaded, serving from", "%s", config->getContentPath().c_str());
    // logger::log_message(logger::STATUS, "Server", std::format("configuration loaded from file: {}", argv[1]));
    // logger::log_message(logger::STATUS, "Server", std::format("serving from: {}", config->getContentPath()));
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