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
    virtual asio::awaitable<void> stream(Socket*) = 0;

    protected:
    long bytes_streamed{0};
};

class StringStreamer: public Streamer
{
    public:
    StringStreamer(const std::string* payload): payload(payload) {}
    asio::awaitable<void> stream(Socket*) override;

    private:
    const std::string* payload;
};

class FileStreamer: public Streamer
{
    public:
    FileStreamer(const std::string& file_path): 
    buffer(BUFFER_SIZE, 0), file_path(file_path), filefd(-1) {openFile();}
    ~FileStreamer() override;
    long getFileSize() {return file_len;}
    asio::awaitable<void> stream(Socket*) override;

    private:
    void openFile();

    private:
    std::vector<char> buffer;
    std::string file_path;
    long file_len;
    int filefd;
};

class ScriptStreamer: public Streamer
{
    public:
    ScriptStreamer(const std::string& script_path, const std::string& stdin_data, 
    std::function<void(const char*, std::size_t)> chunk_callback = nullptr) 
    : script_path(script_path), stdin_data(stdin_data), chunk_callback(chunk_callback) {} 
    ~ScriptStreamer();

    asio::awaitable<void> stream(Socket* sock) override;

    private:
    void spawn();
    void spawnProcess();

    private:
    const std::string& script_path;
    const std::string& stdin_data;
    std::function<void(const char*, std::size_t)> chunk_callback;
    int stdin_pipe[2];
    int stdout_pipe[2];
    int status;
    pid_t pid;
};

#endif