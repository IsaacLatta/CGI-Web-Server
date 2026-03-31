#include "logger/Sink.h"

#include <ctime>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "logger/Entry.h"

namespace {
    std::string get_date() {
        std::string fmt_time = logger::get_time();
        return fmt_time.substr(0, fmt_time.find(" "));
    }
}

namespace logger {

FileSink::FileSink(const std::string& path, const std::string& server_name)
    : fd(-1), date_today_(get_date()), path_(path), server_name_(server_name) {
    OpenFile();
}

FileSink::~FileSink() {
    if(fd != -1) {
        ::close(fd);
    }
}

void FileSink::RotateFile() {
    if(fd != -1) {
        ::close(fd);
    }
    OpenFile();
}

void FileSink::OpenFile() {
    file_name_ = path_ + "/" + server_name_ + "-" + get_date() + ".log";
    if ((fd = ::open(file_name_.c_str(), O_WRONLY | O_CREAT | O_APPEND, 0644))  < 0) {
        perror("openFile failed");
    }
    date_today_ = get_date();
}

void FileSink::Write(const std::string& log_msg) {
    if(date_today_ != get_date()) {
        RotateFile();
    }

    size_t bytes_written { 0u };
    const size_t msg_size { log_msg.length() };
    while(bytes_written < msg_size) {
        const ssize_t bytes = ::write(fd, log_msg.c_str() + bytes_written, msg_size - bytes_written);
        if(bytes < 0) {
            perror("write error: file sink");
            break;
        }
        bytes_written += bytes;
    }
}

void ConsoleSink::Write(const std::string& log_msg) {
    const size_t msg_len = log_msg.length();
    size_t bytes_written { 0u };
    while(bytes_written < msg_len) {
        const ssize_t bytes = ::write(STDOUT_FILENO, log_msg.c_str() + bytes_written, msg_len - bytes_written);
        if(bytes < 0) {
            perror("write error: console sink");
            break;
        }
        bytes_written += bytes;
    }
}
}