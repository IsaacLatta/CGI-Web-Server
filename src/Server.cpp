#include "Server.h"

Server::Server(int local_port, const std::string& cert_path, const std::string& key_path, bool ssl) 
    : _ssl_context(asio::ssl::context::tlsv12), 
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
}

void Server::loadCertificate(const std::string& cert_path, const std::string& key_path)
{
    this->_ssl_context.set_options(asio::ssl::context::default_workarounds | // workaround common bugs
                                  asio::ssl::context::no_sslv2 | // disable sslv2
                                  asio::ssl::context::single_dh_use); // enable new dh use for each session
    this->_ssl_context.use_certificate_chain_file(cert_path);
    this->_ssl_context.use_private_key_file(key_path, asio::ssl::context::pem); // privacy enhanced mail format
}

void Server::run()
{
    std::shared_ptr<Session> session = std::make_shared<Session>(createSocket());
    this->_acceptor->async_accept(session->getSocket()->get_raw_socket(),
    [this, session](const asio::error_code& error)
    {
        this->acceptHandler(error, session);
    });
    
    DEBUG("server", "running");
    this->_io_context.run();
}

void Server::acceptCaller(std::shared_ptr<Session> session)
{
    this->_acceptor->async_accept(session->getSocket()->get_raw_socket(),
    [this, session](const asio::error_code& error)
    {
        this->acceptHandler(error, session);
    });
}

bool Server::isError(const asio::error_code& error)
{
    if(!error)
    {
        _retries = 0;
        return false;
    }

    ERROR("server async_accept", error.value(), error.message(), "");
    if(_retries > MAX_RETRIES || error.value() == asio::error::bad_descriptor || 
        asio::error::access_denied || asio::error::address_in_use)
    {
        EXIT_FATAL("server", error.value(), error.message(), "");
        return true;
    }

    if(error.value() == asio::error::would_block || asio::error::try_again || asio::error::network_unreachable ||
      error.value() == asio::error::connection_refused || asio::error::timed_out || asio::error::no_buffer_space ||
      error.value() == asio::error::host_unreachable)
    {
        this->_retries++;
        std::size_t backoff_time = DEFAULT_BACKOFF_MS * _retries; 
        ERROR("server", error.value(), error.message(), "backing off for: %d ms", backoff_time);
        sleep(backoff_time);
    }

    return false;
}

void Server::acceptHandler(const asio::error_code& error, const std::shared_ptr<Session>& session)
{
    if(isError(error))
        return;

    session->start(); 
    acceptCaller(std::make_shared<Session>(createSocket()));
}

std::unique_ptr<Socket> Server::createSocket()
{
    if (this->_ssl)
        return std::make_unique<HTTPSSocket>(this->_io_context, this->_ssl_context);

    return std::make_unique<HTTPSocket>(this->_io_context);
}
