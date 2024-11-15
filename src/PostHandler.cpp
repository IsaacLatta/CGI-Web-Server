#include "RequestHandler.h" 
#include "Session.h"

extern char** environ;

/* Redirect the child stdin to the read end of the stdin pipe(0), parent will write to the write end(1) */
/* Redirect the child stdout to the write end of the stdout pipe(1), parent will read form the read end(0) */
bool PostHandler::runProcess(int* stdin_pipe, int* stdout_pipe, pid_t* pid, int* status) {
    posix_spawn_file_actions_t actions;
    posix_spawn_file_actions_init(&actions);
    
    if (posix_spawn_file_actions_adddup2(&actions, stdin_pipe[0], STDIN_FILENO) != 0 ||
        posix_spawn_file_actions_adddup2(&actions, stdout_pipe[1], STDOUT_FILENO) != 0) {
        posix_spawn_file_actions_destroy(&actions);
        return false;
    }

    if (posix_spawn_file_actions_addclose(&actions, stdin_pipe[1]) != 0 ||
        posix_spawn_file_actions_addclose(&actions, stdout_pipe[0]) != 0) {
        posix_spawn_file_actions_destroy(&actions);
        return false;
    }

    char* argv[] = {const_cast<char*>(active_route->script.c_str()), (char*)0};
    if((*status = posix_spawn(pid, active_route->script.c_str(), &actions, nullptr, argv, environ)) != 0) {
        posix_spawn_file_actions_destroy(&actions);
        return false; 
    }
    posix_spawn_file_actions_destroy(&actions);
    close(stdin_pipe[0]);
    close(stdout_pipe[1]);
    return true;
}

std::optional<asio::posix::stream_descriptor> PostHandler::runScript(const http::json& args, int* pid, int* status, std::string& opt_error_msg) {
    auto this_session = session.lock();
    if(!this_session) {
        opt_error_msg = std::format("Failed to lock session observer in POST handler, aborting launch of subprocess {} for endpoint {}", active_route->script, active_route->endpoint);
        return std::nullopt;
    } 
    auto executor = this_session->getSocket()->getRawSocket().get_executor();

    int stdin_pipe[2], stdout_pipe[2];
    if(pipe(stdin_pipe) < 0 || pipe(stdout_pipe) < 0) {
        opt_error_msg = std::format("Failed to create pipes for subprocess {}, endpoint: {}, errno={} ({})", active_route->script, active_route->endpoint, errno, strerror(errno));  
        close(stdin_pipe[0]);
        close(stdin_pipe[1]);
        close(stdout_pipe[0]);
        close(stdout_pipe[1]);
        return std::nullopt;
    }

    if(!runProcess(stdin_pipe, stdout_pipe, pid, status)) {
        opt_error_msg = std::format("Failed to launch subprocess {} with posix_spawn, endpoint: {}, errno={} ({})", active_route->script, active_route->endpoint, errno, strerror(errno));  
        close(stdin_pipe[0]);
        close(stdin_pipe[1]);
        close(stdout_pipe[0]);
        close(stdout_pipe[1]);
        return std::nullopt;
    } 

    LOG("POST Handler", "INFO", "Endpoint=%s, executing %s (pid = %d) with status %d ...", active_route->endpoint.c_str(), active_route->script.c_str(), *pid, *status);
    
    ssize_t bytes;
    if ((bytes = write(stdin_pipe[1], args.dump().c_str(), args.dump().length())) < 0) {
        opt_error_msg = std::format("Failed to write args to subprocess {} (pid={}), endpoint: {}, errno={}, ({})", active_route->script, *pid, active_route->endpoint, errno, strerror(errno));
        close(stdout_pipe[0]);
        close(stdin_pipe[1]);
        return std::nullopt;
    }
    close(stdin_pipe[1]); // signal eof to child

    return asio::posix::stream_descriptor(executor, stdout_pipe[0]);
}

asio::awaitable<std::optional<http::error>> PostHandler::sendResponse(asio::posix::stream_descriptor& reader) {
    asio::error_code read_ec;
    buffer.resize(BUFFER_SIZE);
    bool first_read = true;
    std::size_t bytes_read, bytes_to_write, bytes_written(0);
    do {
        std::tie(read_ec, bytes_read) = co_await reader.async_read_some(asio::buffer(buffer.data(), buffer.size()), asio::as_tuple(asio::use_awaitable));
        if(first_read) {
            this->response_header = http::extract_header_line(buffer);
            first_read = false;
            if(active_route->is_authenticator) {
                // extract the role
                // generate the token
            }
        }
        
        if(read_ec && read_ec != asio::error::eof) {
            co_return http::error(http::code::Internal_Server_Error, 
            std::format("Failed to read response from subprocess {} endpoint = {} asio::error={}, ({})", 
            active_route->script, active_route->endpoint, read_ec.value(), read_ec.message()));
        }
        
        bytes_to_write = bytes_read;
        while(bytes_written < bytes_read) {
            auto [write_ec, bytes] = co_await sock->co_write(buffer.data() + bytes_written, bytes_to_write - bytes_written);
            if (write_ec == asio::error::connection_reset || write_ec == asio::error::broken_pipe || write_ec == asio::error::eof) {
                co_return http::error(http::code::Client_Closed_Request, "Connection reset by client");
            }
            
            if(write_ec) {
                co_return http::error(http::code::Internal_Server_Error, 
                std::format("Failed to write response from subprocess {}, endpoint {}, asio::error={} ({})", 
                active_route->script, active_route->endpoint, write_ec.value(), write_ec.message()));
            }
            bytes_written += bytes;
        }
        this->total_bytes += bytes_written;
        bytes_written = 0;
    } while(bytes_read > 0 && read_ec != asio::error::eof);

    co_return std::nullopt;
}

asio::awaitable<void> PostHandler::handle() {
    auto this_session = session.lock();
    if(!this_session) {
        ERROR("POST Handler", 0, "NULL", "failed to lock session observer");
        co_return;
    }
    auto config = cfg::Config::getInstance();

    http::code code;
    cfg::Endpoint endpoint;
    if( (code = http::extract_endpoint(buffer, endpoint)) != http::code::OK ) {
        this_session->onError(http::error(code, "Failed to parse POST request"));
        co_return;
    }

    if((active_route = config->findRoute(endpoint)) == nullptr) {
        this_session->onError(http::error(http::code::Not_Found, std::format("Attempt to access endpoint {} with POST, no matching route", endpoint)));
        co_return;
    }
    
    if(http::trim_to_lower(active_route->method) != "post") {
        this_session->onError(http::error(http::code::Method_Not_Allowed, std::format("Attempt to access {} with POST, allowed={}", endpoint, active_route->method)));
        co_return;
    }
    
    /*
    std::string token;    
    if(active_route->is_protected && (code = http::extract_token(buffer, token)) != http::code::OK && (code = http::verify_token(token, active_route->role)) != http::code::OK) {
        this_session->onError(http::error(code, std::format("Authentication failed for protected endpoint {} with method POST", active_route->endpoint)));
        co_return;
    }
    */

    http::json args;
    if((code = http::build_json(buffer, args)) != http::code::OK) {
        this_session->onError(http::error(code, "Failed to build json array"));
        co_return;
    }

    LOG("INFO", "POST Handler", "REQUEST PARSING SUCCEDED\n%s\nPARSED RESULTS Endpoint: %s JSON Args: %s", buffer.data(), endpoint.c_str(), args.dump().c_str());
    
    int pid, status;
    std::string error_msg;
    auto reader_opt = runScript(args, &pid, &status, error_msg);
    if(reader_opt == std::nullopt) {
        this_session->onError(http::error(http::code::Internal_Server_Error, std::move(error_msg)));
        co_return;
    }
    auto& reader = *reader_opt;

    auto error = co_await sendResponse(reader);
    if(error != std::nullopt) {
        waitpid(pid, &status, NULL);
        this_session->onError(std::move(*error));
        co_return;
    }
    
    waitpid(pid, &status, NULL);
    this_session->onCompletion(this->response_header, this->total_bytes);
}

