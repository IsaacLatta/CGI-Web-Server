#ifndef STREAMER_H
#define STREAMER_H

#include <asio/awaitable.hpp>
#include "http.h"
#include "logger_macros.h"
#include "Transaction.h"
#include "Socket.h"

class Streamer
{
    public:
    long long getBytesStreamed() {return bytes_streamed;}
    virtual ~Streamer() = default;
    virtual asio::awaitable<void> prepare(http::Response*) = 0;
    virtual asio::awaitable<void> stream(Socket*) = 0;

    protected:
    long bytes_streamed{0};
};

class StringStreamer: public Streamer
{
    public:
    StringStreamer(const std::string* payload): payload(payload) {}
    asio::awaitable<void> prepare(http::Response*) override;
    asio::awaitable<void> stream(Socket*) override;

    private:
    const std::string* payload;
};

class FileStreamer: public Streamer
{
    public:
    FileStreamer(const std::string& file_path): file_path(file_path) {}
    ~FileStreamer() override;
    asio::awaitable<void> prepare(http::Response*) override;
    asio::awaitable<void> stream(Socket*) override;

    private:
    std::array<char, BUFFER_SIZE> buffer{};
    std::string file_path;
    long file_len;
    int filefd;
};

#endif