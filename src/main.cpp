#include "Server.h"

#define PATH "/home/isaac/WebServer/"
#define PORT 1025

int main(int argc, char** argv)
{

    if(chroot(PATH) < 0)
    {
        std::cout << "chroot failed, shutting down...\n";
        return 0;
    }

    try
    {
        Server web(PORT);
        web.start();
    }
    catch(const std::exception& e)
    {
        std::cerr << e.what() << '\n';
    }
   

    return 0;
}