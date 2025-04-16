#ifndef STREAMER_H
#define STREAMER_H

#include <asio/awaitable.hpp>
#include <functional>
#include <spawn.h>
#include <sys/wait.h>

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

class ScriptStreamer: public Streamer
{
    public:
    ScriptStreamer(const std::string& script_path, const std::string& stdin_data, 
    std::function<void(const char*, std::size_t)> first_read_callback = nullptr) 
    : script_path(script_path), stdin_data(stdin_data), first_read_callback(first_read_callback) {} 
    ~ScriptStreamer();


    asio::awaitable<void> prepare(http::Response*) override;
    asio::awaitable<void> stream(Socket* sock) override;

    private:
    void spawnProcess();

    private:
    const std::string& script_path;
    const std::string& stdin_data;
    std::function<void(const char*, std::size_t)> first_read_callback;
    int stdin_pipe[2];
    int stdout_pipe[2];
    int status;
    pid_t pid;
};

#endif