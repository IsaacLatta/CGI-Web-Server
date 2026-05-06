#pragma once

#include <asio/awaitable.hpp>
#include <functional>
#include <utility>

#include "http/parsing/parse.h"
#include "logger/macros.h"

namespace io {
    class Streamer {
    public:
        virtual ~Streamer() = default;
        virtual Result OpenStream() = 0;
        virtual asio::awaitable<Result> Stream(Socket&) = 0;
        [[nodiscard]] virtual size_t GetBytesStreamed() const = 0;
    };

    class StringStreamer: public Streamer {
    public:
        StringStreamer(const std::string& payload): payload_(payload) {}

        Result OpenStream() override {
            return Result { {}, 0u };
        }

        asio::awaitable<Result> Stream(Socket&) override;

        [[nodiscard]] size_t GetBytesStreamed() const override {
            return bytes_streamed_;
        }

    private:
        size_t bytes_streamed_ { 0u };
        const std::string& payload_;
    };

    class FileStreamer: public Streamer
    {
    public:
        FileStreamer(const std::string& file_path) : buffer_(BUFFER_SIZE, 0), file_path_(file_path) {}

        ~FileStreamer() override;

        Result OpenStream() override;

        [[nodiscard]] size_t GetBytesStreamed() const override {
            return bytes_streamed_;
        }

        [[nodiscard]] long GetFileSize() const {
            return file_len_;
        }

        asio::awaitable<Result> Stream(Socket&) override;

    private:
        std::vector<char> buffer_;
        std::string file_path_;
        long file_len_ { 0 };
        int filefd_ { -1 };
        size_t bytes_streamed_ { 0u };
    };

    class ScriptStreamer: public Streamer {
    public:
        using ChunkCallback = std::function<asio::awaitable<Result>(const char*, size_t)>;

    public:
        ScriptStreamer(const std::string& script_path, const std::string& stdin_data, ChunkCallback callback = {})
        : script_path_(script_path), stdin_data_(stdin_data), chunk_callback_(std::move(callback)) {}

        ~ScriptStreamer() override;

        Result OpenStream() override;

        asio::awaitable<Result> Stream(Socket& sock) override;

        [[nodiscard]] size_t GetBytesStreamed() const override {
            return bytes_streamed_;
        }

    private:
        Result Spawn();
        Result SpawnProcess();

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
}