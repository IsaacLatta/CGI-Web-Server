#ifndef SINK_H
#define SINK_H

#include <string>
#include <sstream>
#include <iomanip>     
#include <ctime>       
#include <chrono>      
#include <fcntl.h>     
#include <unistd.h>    
#include <sys/types.h> 
#include <sys/stat.h>  
#include <iostream>    

namespace logger {
    
    class Sink
    {
        public:
        virtual void write(const std::string& log_msg) = 0;
        virtual ~Sink() = default;
    };

    class ConsoleSink: public Sink
    {
        public:
        void write(const std::string& log_msg) override;
    };

    class FileSink: public Sink 
    {
        public:
        FileSink(const std::string& path, const std::string& server_name);
        ~FileSink();
        void write(const std::string& log_msg) override;

        private:
        int fd;
        std::string date_today;
        std::string path;
        std::string server_name;
        std::string file_name;

        private:
        void openFile();
        void rotateFile();
    };
};

#endif