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

    char* argv[] = {const_cast<char*>(request->route->getScript(request->method).c_str()), (char*)0};
    if((*status = posix_spawn(pid, request->route->getScript(request->method).c_str(), &actions, nullptr, argv, environ)) != 0) {
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
    if((code = http::build_json(*(txn->getBuffer()), args)) != http::code::OK) {
        throw http::HTTPException(code, "Failed to build json array");
    }

    auto executor = sock->getRawSocket().get_executor();

    int stdin_pipe[2], stdout_pipe[2];
    if(pipe(stdin_pipe) < 0 || pipe(stdout_pipe) < 0) {
        close(stdin_pipe[0]);
        close(stdin_pipe[1]);
        close(stdout_pipe[0]);
        close(stdout_pipe[1]);
        throw http::HTTPException(http::code::Internal_Server_Error, std::format("Failed to create pipes for subprocess {}, endpoint: {}, errno={} ({})", 
        request->route->getScript(request->method), request->endpoint_url, errno, strerror(errno)));  
    }

    if(!runProcess(stdin_pipe, stdout_pipe, pid, status)) {
        close(stdin_pipe[0]);
        close(stdin_pipe[1]);
        close(stdout_pipe[0]);
        close(stdout_pipe[1]);
        throw http::HTTPException(http::code::Internal_Server_Error, std::format("Failed to launch subprocess {} with posix_spawn, endpoint: {}, errno={} ({})", 
        request->route->getScript(request->method), request->endpoint_url, errno, strerror(errno)));  
    } 

    // DEBUG("POST Handler", "endpoint[%s], extecuting %s [pid %s] with status %d", request->endpoint_url.c_str(), request->route->getScript().c_str(), *pid, *status);

    ssize_t bytes;
    if ((bytes = write(stdin_pipe[1], args.dump().c_str(), args.dump().length())) < 0) {
        close(stdout_pipe[0]);
        close(stdin_pipe[1]);
        throw http::HTTPException(http::code::Internal_Server_Error, std::format("Failed to write args to subprocess {} (pid={}), endpoint: {}, errno={}, ({})", 
        request->route->getScript(request->method), *pid, request->endpoint_url, errno, strerror(errno)));
    }
    close(stdin_pipe[1]); // signal eof to child

    return asio::posix::stream_descriptor(executor, stdout_pipe[0]);
}

asio::awaitable<void> PostHandler::readResponse(asio::posix::stream_descriptor& reader) {
    asio::error_code read_ec;
    std::size_t bytes_read;
    std::vector<char> buffer(BUFFER_SIZE);
    std::tie(read_ec, bytes_read) = co_await reader.async_read_some(asio::buffer(buffer.data(), buffer.size()), asio::as_tuple(asio::use_awaitable));
    
    if(read_ec && read_ec != asio::error::eof) {
        throw http::HTTPException(http::code::Internal_Server_Error, 
        std::format("Failed to read response from subprocess={} endpoint={} asio::error={}, ({})", 
        request->route->getScript(request->method), request->endpoint_url, read_ec.value(), read_ec.message()));
    }
    buffer.resize(bytes_read);

    response->status_msg = http::extract_header_line(buffer);
    if((response->status = http::extract_status_code(buffer)) == http::code::Bad_Request /*|| 
        http::extract_body(buffer, response->body) != http::code::OK || 
        http::extract_headers(buffer, response->headers) != http::code::OK*/) {

        throw http::HTTPException(http::code::Bad_Gateway, 
        std::format("Failed to parse response from script={}, endpoint={}", request->route->getScript(request->method), request->endpoint_url));
    }

    response->body = http::extract_body(buffer);
    response->headers = http::extract_headers(buffer);

    txn->addBytes(bytes_read);

    DEBUG("POST Handler", "Parsed script: status=%d, header=%s", static_cast<int>(response->status), response->status_msg.c_str());
}

asio::awaitable<void> PostHandler::handle() {
    if(request->route == nullptr) {
        throw http::HTTPException(http::code::Method_Not_Allowed, std::format("No POST route found for endpoint: {}", request->endpoint_url));
    }

    if(!request->route->getScript(request->method).empty()) {
        int pid, status;
        auto reader = runScript(&pid, &status);
        co_await readResponse(reader);
        waitpid(pid, &status, 0);
    }

    response->addHeader("Connection", "close");
    std::string response = txn->response.build();
    co_await txn->sock->co_write(response.data(), response.size());
    co_return; 
   
    // txn->finish = [self = shared_from_this(), txn = this->txn]() -> asio::awaitable<void> {
    //     std::string response = txn->response.build();
    //     co_await txn->sock->co_write(response.data(), response.size());
    //     co_return;            
    // };
    // co_return;
}

