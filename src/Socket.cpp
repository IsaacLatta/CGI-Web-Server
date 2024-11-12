#include "Socket.h"

HTTPSocket::HTTPSocket(asio::io_context& io_context) : _socket(io_context) {}
    
asio::ip::tcp::socket& HTTPSocket::getRawSocket()
{
    return this->_socket;
}

void HTTPSocket::storeIP() {
    if(_socket.is_open()) {
        address = _socket.remote_endpoint().address().to_string();
    }
    else {
        address = "address not available";
    }
}

void HTTPSocket::handshake(const std::function<void(const asio::error_code&)>& callback)
{
    asio::error_code ec;
    callback(ec);
}

void HTTPSocket::write(char* buffer, std::size_t buffer_size, const std::function<void(const asio::error_code&, std::size_t)>& callback)
{
    asio::async_write(this->_socket, asio::buffer(buffer, buffer_size), 
    [this, callback](const std::error_code& error, std::size_t bytes)
    {
        if(callback) callback(error, bytes);
    });
}

void HTTPSocket::read(char* buffer, std::size_t buffer_size, const std::function<void(const asio::error_code&, std::size_t)>& callback)
{
    this->_socket.async_read_some(asio::buffer(buffer, buffer_size),    
    [this, callback](const asio::error_code& error, std::size_t bytes)
    {
        if(callback) callback(error, bytes);
    });
}

asio::awaitable<asio::error_code> HTTPSocket::co_handshake() {
    co_return asio::error_code{};
}

asio::awaitable<std::tuple<asio::error_code, std::size_t>> HTTPSocket::co_read(char* buffer, std::size_t size) {
    auto [ec, bytes_read] = co_await _socket.async_read_some(asio::buffer(buffer, size), asio::as_tuple(asio::use_awaitable));
    co_return std::make_tuple(ec, bytes_read);
}

asio::awaitable<std::tuple<asio::error_code, std::size_t>> HTTPSocket::co_write(const char* buffer, std::size_t size) {
    auto [ec, bytes_written] = co_await asio::async_write(_socket, asio::buffer(buffer, size), asio::as_tuple(asio::use_awaitable));
    co_return std::make_tuple(ec, bytes_written);
}

void HTTPSocket::close()
{
    this->_socket.close();
}

HTTPSocket::~HTTPSocket()
{
    if(this->_socket.is_open())
    {
        this->_socket.close();
    }
}

/*////// HTTPS Socket //////*/

HTTPSSocket::HTTPSSocket(asio::io_context& io_context, asio::ssl::context& ssl_context): _socket(io_context, ssl_context)  {}

void HTTPSSocket::storeIP() {
    if(_socket.next_layer().is_open()) {
        address = _socket.next_layer().remote_endpoint().address().to_string();
    }
    else {
        address = "address not available";
    }
}

asio::ip::tcp::socket& HTTPSSocket::getRawSocket()
{
    return this->_socket.next_layer();
}

void HTTPSSocket::handshake(const std::function<void(const std::error_code& error)>& callback)
{
    this->_socket.async_handshake(asio::ssl::stream_base::server,
    [this, callback](const asio::error_code& error)
    {
        if(callback) callback(error);
    });
}

void HTTPSSocket::read(char* buffer, std::size_t buffer_size, const std::function<void(const asio::error_code&, std::size_t)>& callback)
{
    this->_socket.async_read_some(asio::buffer(buffer, buffer_size),    
    [this, callback](const asio::error_code& error, std::size_t bytes)
    {
        if(callback) callback(error, bytes);
    });
}

void HTTPSSocket::write(char* buffer, std::size_t buffer_size, const std::function<void(const asio::error_code&, std::size_t)>& callback)
{
    asio::async_write(this->_socket, asio::buffer(buffer, buffer_size), 
    [this, callback](const std::error_code& error, std::size_t bytes)
    {
        if(callback) callback(error, bytes);
    });
}

asio::awaitable<asio::error_code> HTTPSSocket::co_handshake() {
    auto [ec] = co_await this->_socket.async_handshake(asio::ssl::stream_base::server, asio::as_tuple(asio::use_awaitable));
    co_return ec;
}

asio::awaitable<std::tuple<asio::error_code, std::size_t>> HTTPSSocket::co_read(char *buffer, std::size_t buffer_size) {
    auto [ec, bytes_read]= co_await _socket.async_read_some(asio::buffer(buffer, buffer_size), asio::as_tuple(asio::use_awaitable));
    co_return std::make_tuple(ec, bytes_read);
}

asio::awaitable<std::tuple<asio::error_code, std::size_t>> HTTPSSocket::co_write(const char *buffer, std::size_t buffer_size) {
    auto [ec, bytes_written] = co_await asio::async_write(_socket, asio::buffer(buffer, buffer_size), asio::as_tuple(asio::use_awaitable));
    co_return std::make_tuple(ec, bytes_written);
}

void HTTPSSocket::close()
{
    this->_socket.next_layer().close();
}

HTTPSSocket::~HTTPSSocket()
{
    if(this->_socket.next_layer().is_open())
    {
        this->_socket.next_layer().close();
    }
}