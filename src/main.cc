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
    return 0;
    // if(argc < 2 && argv[1] == nullptr) {
    //     std::cerr << "ERROR no configuration path, must provide a path to load configuration from\n";
    //     return 0;
    // }
    //
    // const cfg::Config* config = cfg::Config::getInstance(argv[1]);
    // STATUS("Server", "configuration loaded from file: %s", argv[1]);
    // STATUS("Server", "serving from: %s", config->getContentPath().c_str());
    // STATUS("Server", "writing logs to: %s", config->getLogPath().c_str());
    // STATUS("Server", "%s is running on [%s %s:%d] pid=%ld",
    //     config->getServerName().c_str(),
    //     asio::ip::host_name().c_str(),
    //     config->getHostIP().c_str(),
    //     config->getPort(),
    //     getpid());

    // asio::io_context context;
    //
    // try {
    //     http::Server server(config);
    //     server.start();
    // }
    // catch(const std::exception& e) {
    //     std::cerr << e.what() << '\n';
    // }
    //
    // return 0;
}