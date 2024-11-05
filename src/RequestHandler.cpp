#include "RequestHandler.h"
#include "Session.h"

std::unique_ptr<RequestHandler> RequestHandler::handlerFactory(std::weak_ptr<Session> sess, const char* buffer, std::size_t size) {
    std::string request(buffer, size);
    auto session = sess.lock();
    if(!session) {
        ERROR("shared_ptr.lock", 0, "NULL", "failed to lock session ptr");
        return nullptr;
    }

    if(request.find("GET") != std::string::npos || request.find("get") != std::string::npos) {
        return std::make_unique<GetHandler>(session, session->getSocket(), buffer); 
    }
    else {
        return nullptr;
    }
}

asio::awaitable<long> GetHandler::sendHeader(int filefd, const std::string& content_type) {
    long file_len = (long)lseek(filefd, (off_t)0, SEEK_END);
    if(file_len <= 0) {
        co_return -1;
    }
    lseek(filefd, 0, SEEK_SET);
    
    std::string header = "HTTP/1.1 200 OK\r\nContent-Type: " + content_type + "\r\n"
                            "Content-Length: " + std::to_string(file_len) + "\r\n"
                            "Connection: close\r\n\r\n";
    
    auto [error, bytes_written] = co_await sock->co_write(header.data(), header.length());
    if(error) {
        co_return -1;
    }
    co_return file_len;
}

asio::awaitable<void> GetHandler::sendResource(int filefd, long file_len) {
    asio::posix::stream_descriptor file_desc(co_await asio::this_coro::executor, filefd);
    
    std::vector<char> buffer(250 * 1024);  // 250 KB buffer
    long bytes_sent = 0;

    while (bytes_sent < file_len) {
        std::size_t bytes_to_read = std::min(buffer.size(), static_cast<std::size_t>(file_len - bytes_sent));
        std::size_t bytes_to_write = co_await file_desc.async_read_some(asio::buffer(buffer.data(), bytes_to_read), asio::use_awaitable);
        if (bytes_to_write == 0) {
            LOG("WARN", "Get Handler", "EOF reached prematurely while sending resource");
            break;
        }

        std::size_t total_bytes_written = 0;
        while (total_bytes_written < bytes_to_write) {
            auto [ec, bytes_written] = co_await sock->co_write(buffer.data() + total_bytes_written, bytes_to_write - total_bytes_written);
            if (ec) {
                ERROR("Get Handler", ec.value(), ec.message(), "Failed to send resource");
                close(filefd);
                co_return;
            }
            total_bytes_written += bytes_written;
        }

        bytes_sent += total_bytes_written;
    }
    
    close(filefd);
    LOG("INFO", "Get Handler", "Finished sending file, file size: %ld, bytes sent: %ld", file_len, bytes_sent);
}


asio::awaitable<void> GetHandler::fillRequest(const std::string& resource, const std::string& content_type) {
    int filefd =  open(resource.c_str(), O_RDONLY | O_NONBLOCK);
    if(filefd == -1) {
        ERROR("Get Handler", errno, "", "failed to open resource: %s", resource.c_str());
        co_return;
    }
    
    std::size_t file_len = co_await sendHeader(filefd, content_type);
    if(file_len < 0) {
        ERROR("Get Handler", errno, "", "failed to send header");
        close(filefd);
        co_return;
    }
    co_await sendResource(filefd, file_len);
}

asio::awaitable<void> GetHandler::handle() {
    auto this_session = session.lock();
    if(!this_session) {
        ERROR("Get Handler", 0, "NULL", "no session available");
        co_return;
    }
    
    http::code code;
    http::clean_buffer(buffer);
    if((code = http::validate_buffer(buffer)) != http::code::OK || (code = http::validate_method(buffer)) != http::code::OK  ) {
        ERROR("Get Handler", code, "http", "REQUEST\n%s", buffer.data());
        this_session->onError();
        co_return;
    }

    std::string resource, content_type;
    if((code = http::extract_resource(buffer, resource)) != http::code::OK || (code = http::extract_content_type(resource, content_type)) != http::code::OK) {
        ERROR("Get Handler", code, "http", "REQUEST\n%s", buffer.data());
        this_session->onError();
        co_return;
    }

    LOG("INFO", "Get Handler", "Request Received\nResource: %s\nContent_Type: %s", resource.c_str(), content_type.c_str());
    
    co_await fillRequest(resource, content_type);
    this_session->onCompletion();
}
