#ifndef STREAMER_H
#define STREAMER_H

#include <asio/awaitable.hpp>
#include <functional>
#include <utility>
#include <spawn.h>
#include <sys/wait.h>

#include "http/parsing/parse.h"
#include "logger/macros.h"
#include "http/Transaction.h"
#include "io/Socket.h"

class Streamer {
public:
    virtual ~Streamer() = default;
    virtual asio::awaitable<void> Stream(io::Socket&) = 0;
    virtual size_t BytesStreamed() const = 0;
};

class StringStreamer: public Streamer {
public:
    StringStreamer(const std::string& payload): payload_(payload) {}

    asio::awaitable<void> Stream(io::Socket&) override;

    size_t BytesStreamed() const override {
        return bytes_streamed_;
    }

private:
    size_t bytes_streamed_ { 0u };
    const std::string& payload_;
};

class FileStreamer: public Streamer
{
public:
    FileStreamer(const std::string& file_path) : buffer_(io::BUFFER_SIZE, 0), file_path_(file_path) {
        openFile();
    }

    ~FileStreamer() override;

    size_t BytesStreamed() const override {
        return bytes_streamed_;
    }

    long GetFileSize() const {
        return file_len_;
    }

    asio::awaitable<void> Stream(io::Socket&) override;

private:
    void openFile();

private:
    std::vector<char> buffer_;
    std::string file_path_;
    long file_len_ { 0 };
    int filefd_ { -1 };
    size_t bytes_streamed_ { 0u };
};

class ScriptStreamer: public Streamer {
public:
    using ChunkCallback = std::function<asio::awaitable<void>(const char*, std::size_t)>;

public:
    ScriptStreamer(const std::string& script_path, const std::string& stdin_data, ChunkCallback callback)
    : script_path_(script_path), stdin_data_(stdin_data), chunk_callback_(std::move(callback)) {}

    ~ScriptStreamer() override;

    asio::awaitable<void> Stream(io::Socket& sock) override;

    size_t BytesStreamed() const override {
        return bytes_streamed_;
    }

private:
    void Spawn();
    void SpawnProcess();

private:
    const std::string& script_path_;
    const std::string& stdin_data_;
    ChunkCallback chunk_callback_;
    int stdin_pipe_[2] {};
    int stdout_pipe_[2] {};
    int status_ { -1 };
    pid_t pid_ { -1 };
    size_t bytes_streamed_ { 0u };
};

#endif