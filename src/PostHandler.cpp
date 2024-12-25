#include "MethodHandler.h"
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

asio::posix::stream_descriptor PostHandler::runScript(int* pid, int* status) {
    http::json args;
    http::code code;
    std::vector<char> buffer;
    if((code = http::build_json(buffer, args)) != http::code::OK) {
        throw http::HTTPException(code, "Failed to build json array");
    }

    auto executor = sock->getRawSocket().get_executor();

    int stdin_pipe[2], stdout_pipe[2];
    if(pipe(stdin_pipe) < 0 || pipe(stdout_pipe) < 0) {
        close(stdin_pipe[0]);
        close(stdin_pipe[1]);
        close(stdout_pipe[0]);
        close(stdout_pipe[1]);
        throw http::HTTPException(http::code::Internal_Server_Error, std::format("Failed to create pipes for subprocess {}, endpoint: {}, errno={} ({})", active_route->script, active_route->endpoint, errno, strerror(errno)));  
    }

    if(!runProcess(stdin_pipe, stdout_pipe, pid, status)) {
        close(stdin_pipe[0]);
        close(stdin_pipe[1]);
        close(stdout_pipe[0]);
        close(stdout_pipe[1]);
        throw http::HTTPException(http::code::Internal_Server_Error, std::format("Failed to launch subprocess {} with posix_spawn, endpoint: {}, errno={} ({})", active_route->script, active_route->endpoint, errno, strerror(errno)));  
    } 

    LOG("POST Handler", "INFO", "Endpoint=%s, executing %s (pid = %d) with status %d ...", active_route->endpoint.c_str(), active_route->script.c_str(), *pid, *status);
    
    ssize_t bytes;
    if ((bytes = write(stdin_pipe[1], args.dump().c_str(), args.dump().length())) < 0) {
        close(stdout_pipe[0]);
        close(stdin_pipe[1]);
        throw http::HTTPException(http::code::Internal_Server_Error, std::format("Failed to write args to subprocess {} (pid={}), endpoint: {}, errno={}, ({})", active_route->script, *pid, active_route->endpoint, errno, strerror(errno)));
    }
    close(stdin_pipe[1]); // signal eof to child

    return asio::posix::stream_descriptor(executor, stdout_pipe[0]);
}

asio::awaitable<void> PostHandler::sendResponse(asio::posix::stream_descriptor& reader) {
    asio::error_code read_ec;
    std::vector<char> buffer(BUFFER_SIZE);
    bool first_read = true;
    std::size_t bytes_read, bytes_to_write, bytes_written(0);
    do {
        std::tie(read_ec, bytes_read) = co_await reader.async_read_some(asio::buffer(buffer.data(), buffer.size()), asio::as_tuple(asio::use_awaitable));
        if(first_read) {
            this->response_header = http::extract_header_line(buffer);
            first_read = false;
            // in the Authenticator on the pass back we should parse this header to see if the user is authenticated, generate the token if so
        }
        
        if(read_ec && read_ec != asio::error::eof) {
            throw http::HTTPException(http::code::Internal_Server_Error, 
            std::format("Failed to read response from subprocess {} endpoint = {} asio::error={}, ({})", 
            active_route->script, active_route->endpoint, read_ec.value(), read_ec.message()));
        }
        
        bytes_to_write = bytes_read;
        while(bytes_written < bytes_read) {
            auto [write_ec, bytes] = co_await sock->co_write(buffer.data() + bytes_written, bytes_to_write - bytes_written);
            if (write_ec == asio::error::connection_reset || write_ec == asio::error::broken_pipe || write_ec == asio::error::eof) {
                throw http::HTTPException(http::code::Client_Closed_Request, "Connection reset by client");
            }
            
            if(write_ec) {
                throw http::HTTPException(http::code::Internal_Server_Error, 
                std::format("Failed to write response from subprocess {}, endpoint {}, asio::error={} ({})", 
                active_route->script, active_route->endpoint, write_ec.value(), write_ec.message()));
            }
            bytes_written += bytes;
        }
        this->total_bytes += bytes_written;
        bytes_written = 0;
    } while(bytes_read > 0 && read_ec != asio::error::eof);
}

void PostHandler::handleEmptyScript() {
    http::json response_body;
    std::string token = "";
    
    response->setStatus(http::code::OK);
    response->addHeader("Connection", "close");
}

asio::awaitable<void> PostHandler::handle() {
    if(request->route == nullptr) {
        throw http::HTTPException(http::code::Bad_Request, std::format("No POST route found for endpoint: {}", request->endpoint));
    }
    
    int pid, status;
    auto reader = runScript(&pid, &status);
    co_await sendResponse(reader);
    waitpid(pid, &status, 0);
    co_return;
}

