#include "Server.h"

Server::Server(config::ServerConfig server_config, int local_port, const std::string& cert_path, const std::string& key_path, bool ssl) 
    : 
    _config(server_config),
    _ssl_context(asio::ssl::context::tlsv12), 
    _port(local_port), 
    _ssl(ssl),
    _retries(0)
{
    this->_endpoint = asio::ip::tcp::endpoint(asio::ip::tcp::v4(), this->_port);
    _acceptor = std::make_shared<asio::ip::tcp::acceptor>(this->_io_context, this->_endpoint);

    if(_ssl && !cert_path.empty() && !key_path.empty())
    {
        loadCertificate(cert_path, key_path);
    }
    print_routes(_config.routes);
}

void Server::loadCertificate(const std::string& cert_path, const std::string& key_path)
{
    this->_ssl_context.set_options(asio::ssl::context::default_workarounds | // workaround common bugs
                                  asio::ssl::context::no_sslv2 | // disable sslv2
                                  asio::ssl::context::single_dh_use); // enable new dh use for each session
    this->_ssl_context.use_certificate_chain_file(cert_path);
    this->_ssl_context.use_private_key_file(key_path, asio::ssl::context::pem); // privacy enhanced mail format
}

asio::awaitable<void> Server::run() {
    std::error_code ec;
    
    LOG("INFO", "server", "running on port %d ...", _port);
    while(true) {
        auto session = std::make_shared<Session>(createSocket(), _config.routes);

        co_await _acceptor->async_accept(session->getSocket()->getRawSocket(), asio::redirect_error(asio::use_awaitable, ec));
        if(isError(ec)) {
            co_return;
        }

        LOG("INFO", "server", "starting session for client: %s", session->getSocket()->getIP().c_str());
        co_await session->start();
    }
}

void Server::start() {
    asio::co_spawn(_io_context, run(), asio::detached);
    _io_context.run();
}

bool Server::isError(const asio::error_code& error)
{
    if(!error)
    {
        _retries = 0;
        return false;
    }

    ERROR("server async_accept", error.value(), error.message().c_str(), "");
    if(_retries > MAX_RETRIES || error.value() == asio::error::bad_descriptor || 
        error.value() == asio::error::access_denied || error.value() == asio::error::address_in_use)
    {
        EXIT_FATAL("server", error.value(), error.message().c_str(), "");
        return true;
    }

    if(error.value() == asio::error::would_block || error.value() == asio::error::try_again || error.value() == asio::error::network_unreachable ||
      error.value() == asio::error::connection_refused || error.value() == asio::error::timed_out || error.value() == asio::error::no_buffer_space ||
      error.value() == asio::error::host_unreachable)
    {
        this->_retries++;
        std::size_t backoff_time = DEFAULT_BACKOFF_MS * _retries; 
        ERROR("server", error.value(), error.message().c_str(), "backing off for: %ld ms", backoff_time);
        sleep(backoff_time);
    }

    return false;
}

std::unique_ptr<Socket> Server::createSocket()
{
    if (this->_ssl)
        return std::make_unique<HTTPSSocket>(this->_io_context, this->_ssl_context);

    return std::make_unique<HTTPSocket>(this->_io_context);
}
