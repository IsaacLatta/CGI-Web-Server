#pragma once

#include <string>

#include "logger/forward.h"

namespace logger {

class Sink {
public:
    virtual void Write(const std::string&) = 0;
    virtual ~Sink() = default;
};

class ConsoleSink: public Sink {
public:
    void Write(const std::string&) override;
};

class FileSink: public Sink {
public:
    FileSink(const std::string& path, const std::string& server_name);
    ~FileSink() override;

    void Write(const std::string&) override;

private:
    void OpenFile();
    void RotateFile();

private:
    int fd { -1 };
    std::string date_today_;
    std::string path_;
    std::string server_name_;
    std::string file_name_;
};

}