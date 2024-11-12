#include "Session.h"

asio::awaitable<void> Session::start() {
    auto self = shared_from_this();
    session_info.start_time = std::chrono::system_clock::now();
    
    asio::error_code error = co_await sock->co_handshake();
    if(error) {
        ERROR("co_handshake", error.value(), error.message().c_str() , "session failed to start");
        co_return;
    }

    std::array<char, HEADER_SIZE> buffer;
    auto [ec, bytes_read] = co_await sock->co_read(buffer.data(), buffer.size());
    if(ec) {
        ERROR("co_read", ec.value(), ec.message().c_str() , "failed to read header");
        co_return;
    }
    
    if((handler = RequestHandler::handlerFactory(weak_from_this(), buffer.data(), bytes_read))) {
        begin(buffer.data(), bytes_read);
        co_await handler->handle();
    }
}

void Session::begin(const char* header, std::size_t size) {
    sock->storeIP();
    session_info.user_agent = logger::get_user_agent(header, size);
    session_info.request = logger::get_header_line(header, size);
    session_info.RTT_start_time = std::chrono::system_clock::now();
    session_info.client_addr = sock->getIP();
}

void Session::onError(http::error&& ec) {
    char buffer[BUFSIZ];
    snprintf(buffer, BUFSIZ, "Invoked for client %s", sock->getIP().c_str());
    ERROR(buffer, static_cast<int>(ec.error_code), ec.message.c_str(), "");
    
    session_info.end_time = std::chrono::system_clock::now();
    session_info.response = ec.response;
    logger::log_file(session_info, "ERROR");

    if(ec.error_code != http::code::Client_Closed_Request) {
        sock->write(ec.response.data(), ec.response.length());
    }
    sock->close();
}

void Session::onCompletion(const std::string& response, long bytes_moved) {
    session_info.bytes = bytes_moved;
    session_info.response = logger::get_header_line(response.data(), response.length());
    session_info.end_time = std::chrono::system_clock::now();
    logger::log_file(session_info, "INFO");
    sock->close();
}