#include "RequestHandler.h"
#include "Session.h"

std::unique_ptr<RequestHandler> RequestHandler::handlerFactory(std::weak_ptr<Session> sess, const char* buffer, std::size_t size) {
    auto session = sess.lock();
    if(!session) {
        ERROR("Get Handler", 0, "NULL", "session observer is NULL");
        return nullptr;
    }
    
    std::string request(buffer, size);
    if(!session) {
        ERROR("shared_ptr.lock", 0, "NULL", "failed to lock session ptr");
        return nullptr;
    }

    if(request.find("GET") != std::string::npos || request.find("get") != std::string::npos) {
        LOG("INFO", "Request Handler", "Get request detected");
        return std::make_unique<GetHandler>(sess, session->getSocket(), buffer, size); 
    }
    return nullptr;
}

std::string GetHandler::buildHeader(int filefd, const std::string& content_type, long& file_len) {
    file_len = (long)lseek(filefd, (off_t)0, SEEK_END);
    if(file_len <= 0) {
        return "";
    }
    lseek(filefd, 0, SEEK_SET);
    
    std::string header = "HTTP/1.1 200 OK\r\nContent-Type: " + content_type + "\r\n"
                            "Content-Length: " + std::to_string(file_len) + "\r\n"
                            "Connection: close\r\n\r\n";
    return header;
}

asio::awaitable<void> GetHandler::sendResource(int filefd, long file_len) {
    auto this_session = session.lock();
    if(!this_session) {
        ERROR("Get Handler", 0, "NULL", "session observer is null");
        co_return;
    }
    
    asio::posix::stream_descriptor file_desc(co_await asio::this_coro::executor, filefd);
    
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
                ERROR("Get Handler", ec.value(), ec.message().c_str(), "Failed to send resource");
                close(filefd);
                co_return;
            }
            total_bytes_written += bytes_written;
        }

        bytes_sent += total_bytes_written;
    }
    
    close(filefd);
    LOG("INFO", "Get Handler", "Finished sending file, file size: %ld, bytes sent: %ld", file_len, bytes_sent);
    this_session->onCompletion();
}


asio::awaitable<void> GetHandler::handle() {
    auto this_session = session.lock();
    if(!this_session) {
        ERROR("Get Handler", 0, "NULL", "session observer is null");
        co_return;
    }

    http::code code;
    http::clean_buffer(buffer);
    std::string resource, content_type;
    if((code = http::extract_resource(buffer, resource)) != http::code::OK || (code = http::extract_content_type(resource, content_type)) != http::code::OK) {
        ERROR("Get Handler", static_cast<int>(code), "http", "REQUEST\n%s", buffer.data());
        this_session->onError(http::error(code));
        co_return;
    }

    LOG("INFO", "Get Handler", "Request Received\nResource: %s\nContent_Type: %s", resource.c_str(), content_type.c_str());
    
    int filefd =  open(resource.c_str(), O_RDONLY | O_NONBLOCK);
    if(filefd == -1) {
        ERROR("Get Handler", errno, "", "failed to open resource: %s", resource.c_str());
        co_return;
    }
    
    long file_len;
    std::string header = buildHeader(filefd, content_type, file_len);
    if(file_len < 0) {
        ERROR("Get Handler", errno, "", "failed to send header");
        close(filefd);
        co_return;
    }

    auto [error, bytes_written] = co_await sock->co_write(header.data(), header.length());
    if(error) {
        ERROR("Get Handler", error.value(), error.message().c_str(), "failed send header");
        co_return;
    }

    co_await sendResource(filefd, file_len);
}
