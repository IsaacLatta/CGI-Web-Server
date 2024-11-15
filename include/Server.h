#ifndef REVPROXY_H
#define REVPROXY_H

#include <asio.hpp>
#include <asio/co_spawn.hpp>
#include <asio/detached.hpp>
#include <asio/use_awaitable.hpp>
#include <string>
#include <memory>
#include "Session.h"
#include "config.h"

#define DEFAULT_BACKOFF_MS 100 
#define MAX_RETRIES 5

class Server
{
    public:
    Server(const cfg::Config* server_config, int local_port, const std::string& cert_path = "", const std::string& key_path = "", bool ssl = false);
    void start();
    asio::awaitable<void> run();

    private:
    void loadCertificate(const std::string& cert_path, const std::string& key_path);
    bool isError(const asio::error_code& error);
    std::unique_ptr<Socket> createSocket();

    private:
    const cfg::Config* _config;
    asio::io_context _io_context;
    asio::ssl::context _ssl_context;
    std::shared_ptr<asio::ip::tcp::acceptor> _acceptor;
    asio::ip::tcp::endpoint _endpoint;
    int _port;
    bool _ssl;
    std::size_t _retries;

};

#endif