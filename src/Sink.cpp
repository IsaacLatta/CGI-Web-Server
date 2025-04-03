#include "Sink.h"
#include "logger.h"

using namespace logger;

static std::string get_date() {
    std::string fmt_time = logger::get_time();
    return fmt_time.substr(0, fmt_time.find(" "));
}

logger::FileSink::FileSink(const std::string& path, const std::string& server_name) 
    :fd(-1), date_today(get_date()), path(path), server_name(server_name)  
{
    openFile();
}

logger::FileSink::~FileSink() {
    if(fd != -1) {
        ::close(fd);
    }
}

void logger::FileSink::rotateFile() {
    if(fd != -1) {
        ::close(fd);
    }
    openFile();
}

void logger::FileSink::openFile() {
    file_name = path + "/" + server_name + "-" + get_date() + ".log";
    if ((fd = ::open(file_name.c_str(), O_WRONLY | O_CREAT | O_APPEND, 0644))  < 0) {
        perror("openFile failed");
    }
    date_today = get_date();
}

void logger::FileSink::write(const std::string& log_msg) {
    if(date_today != get_date()) {
        rotateFile();
    }
    
    std::size_t bytes_written = 0, msg_size = log_msg.length();
    while(bytes_written < msg_size) {
        ssize_t bytes = ::write(fd, log_msg.c_str() + bytes_written, msg_size - bytes_written);
        if(bytes < 0) {
            perror("write failed");
            break; 
        }
        bytes_written += bytes;
    }
}

void ConsoleSink::write(const std::string& log_msg) {
    std::size_t msg_len = log_msg.length(), bytes_written = 0;
    while(bytes_written < msg_len) {
        ssize_t bytes = ::write(STDOUT_FILENO, log_msg.c_str() + bytes_written, msg_len - bytes_written);
        if(bytes < 0) {
            perror("write error: console sink");
            break;
        }
        bytes_written += bytes;
    }
}

