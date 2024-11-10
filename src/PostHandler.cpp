#include "RequestHandler.h" 
#include "Session.h"

extern char** environ;


/* Redirect the child stdin to the read end of the stdin pipe(0), parent will write to the write end(1) */
/* Redirect the child stdout to the write end of the stdout pipe(1), parent will read form the read end(0) */
bool PostHandler::runSubprocess(int* stdin_pipe, int* stdout_pipe, pid_t* pid, int* status) {
    posix_spawn_file_actions_t actions;
    posix_spawn_file_actions_init(&actions);
    posix_spawn_file_actions_adddup2(&actions, stdin_pipe[0], STDIN_FILENO); 
    posix_spawn_file_actions_adddup2(&actions, stdout_pipe[1], STDOUT_FILENO); 
    posix_spawn_file_actions_addclose(&actions, stdin_pipe[1]);
    posix_spawn_file_actions_addclose(&actions, stdout_pipe[0]);

    std::array<char*, 3> args = {const_cast<char*>("python3"), const_cast<char*>(active_route.script.c_str()), nullptr};
    if(*status = posix_spawn(pid, "/usr/bin/python3", &actions, nullptr, args.data(), environ) != 0) {
        posix_spawn_file_actions_destroy(&actions);
        close(stdin_pipe[0]);
        close(stdin_pipe[1]);
        close(stdout_pipe[0]);
        close(stdout_pipe[1]);
        return false; 
    }
    posix_spawn_file_actions_destroy(&actions);
    close(stdin_pipe[0]);
    close(stdout_pipe[1]);
    return true;
}

asio::awaitable<void> PostHandler::runScript(const std::string& json_args) {
    auto this_session = session.lock();
    if(!this_session) {
        ERROR("POST Handler", 0, "NULL", "failed to lock session observer");
        co_return;
    } 

    int stdin_pipe[2], stdout_pipe[2];
    if(pipe(stdin_pipe) < 0 || pipe(stdout_pipe)) {
        this_session->onError(http::error(http::code::Internal_Server_Error, 
        std::format("Failed to create pipes for subprocess {}, endpoint: {}, errno={} ({})", active_route.script, active_route.endpoint, errno, strerror(errno))));  
        co_return;  
    }
    int status; pid_t pid; 
    if(!runSubprocess(stdin_pipe, stdout_pipe, &pid, &status)) {
        this_session->onError(http::error(http::code::Internal_Server_Error, std::format("Failed to launch subprocess {} with posix_spawn, endpoint: {}, errno={} ({})", active_route.script, active_route.endpoint, errno, strerror(errno))));  
    } 

    auto executor = this_session->getSocket()->getRawSocket().get_executor();
    asio::posix::stream_descriptor write_stream(executor, stdin_pipe[1]);
    asio::posix::stream_descriptor read_stream(executor, stdout_pipe[0]);

    std::array<char, BUFSIZ> script_bufer;

}

asio::awaitable<void> PostHandler::handle() {
    auto this_session = session.lock();
    if(!this_session) {
        ERROR("POST Handler", 0, "NULL", "failed to lock session observer");
        co_return;
    }

    http::code code;
    std::string endpoint;
    if( (code = http::extract_endpoint(buffer, endpoint)) != http::code::OK ) {
        this_session->onError(http::error(code, "Failed to parse POST request"));
        co_return;
    }

    active_route = routes[endpoint];
    if(active_route.method != "POST" && active_route.method != "post") {
        this_session->onError(http::error(http::code::Method_Not_Allowed, std::format("Attempt to access {} with POST, allowed={}", endpoint, active_route.method)));
        co_return;
    }
    
    http::json args;
    if((code = http::build_json(buffer, args)) != http::code::OK) {
        this_session->onError(http::error(code, "Failed to build json array"));
        co_return;
    }

    LOG("INFO", "POST Handler", "REQUEST PARSING SUCCEDED\n%s\n\nPARSED RESULTS\nEndpoint: %s\nJSON Args: %s", buffer.data(), endpoint.c_str(), args.dump().c_str());
    //runScript(args);
}

