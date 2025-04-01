#include "Server.h"

#include <asio.hpp>
#include <iostream>
#include <string>

Server::Server(const cfg::Config* server_config) 
    : _config(server_config),
      _io_context(),
      _ssl_context(asio::ssl::context::tlsv12),
      _acceptor(),
      _endpoint(asio::ip::tcp::v4(), server_config->getPort()),
      _ssl(_config->getSSL()->active),
      _retries(0)
{
    this->_acceptor = std::make_shared<asio::ip::tcp::acceptor>(this->_io_context, this->_endpoint);

    if(_ssl) {
        loadCertificate();
    }
}

void Server::loadCertificate() {
    auto ssl_config = _config->getSSL();
    this->_ssl_context.set_options(asio::ssl::context::default_workarounds | // workaround common bugs
                                  asio::ssl::context::no_sslv2 | // disable sslv2
                                  asio::ssl::context::single_dh_use); // enable new dh use for each session
    this->_ssl_context.use_certificate_chain_file(ssl_config->certificate_path);
    this->_ssl_context.use_private_key_file(ssl_config->key_path, asio::ssl::context::pem); // privacy enhanced mail format
}

asio::awaitable<void> Server::run() {
    STATUS("Server", "%s is running on [%s %s:%d] pid=%ld", _config->getServerName().c_str(), asio::ip::host_name().c_str(), _config->getHostIP().c_str(), _config->getPort(), getpid());
    std::error_code ec;
    while(true) {
        auto session = std::make_shared<Session>(createSocket());

        co_await _acceptor->async_accept(session->getSocket()->getRawSocket(), asio::redirect_error(asio::use_awaitable, ec));
        if(isError(ec)) {
            co_return;
        }

        co_await session->start();
    }
}

void Server::start() {
    /* This thread, 1 logger thread, the rest of the threads are workers*/
    std::vector<std::thread> threads;
    std::size_t thread_count = std::thread::hardware_concurrency();
    if(!thread_count) {
        thread_count = 8;
    }
    threads.reserve(thread_count - 2); // -1 for this, -1 for the logger = -2

    asio::co_spawn(_io_context, run(), asio::detached);

    for(std::size_t i = 0; i < thread_count - 2; ++i) {
        threads.emplace_back([this](){_io_context.run();});
    }
    _io_context.run();

    for(auto& thread: threads) {
        thread.join();
    }
}

bool Server::isError(const asio::error_code& error) {
    if(!error) {
        _retries = 0;
        return false;
    }

    DEBUG("Server", "async accept: error=%d %s", error.value(), error.message().c_str());
    if(_retries > MAX_RETRIES || error.value() == asio::error::bad_descriptor || 
        error.value() == asio::error::access_denied || error.value() == asio::error::address_in_use)
    {
        FATAL("Server", "error=%d %s", error.value(), error.message().c_str());
    }

    if(error.value() == asio::error::would_block || error.value() == asio::error::try_again || error.value() == asio::error::network_unreachable ||
      error.value() == asio::error::connection_refused || error.value() == asio::error::timed_out || error.value() == asio::error::no_buffer_space ||
      error.value() == asio::error::host_unreachable)
    {
        this->_retries++;
        std::size_t backoff_time_ms = DEFAULT_BACKOFF_MS * _retries; 
        WARN("Server", "error=%d %s, backing off for %ld ms", error.value(), error.message().c_str(), backoff_time_ms);
        sleep(backoff_time_ms);
    }

    return false;
}

std::unique_ptr<Socket> Server::createSocket()
{
    if (this->_ssl)
        return std::make_unique<HTTPSSocket>(this->_io_context, this->_ssl_context);

    return std::make_unique<HTTPSocket>(this->_io_context);
}
