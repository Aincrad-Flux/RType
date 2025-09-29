#include "UdpServer.hpp"
#include <iostream>
#include <csignal>
#include <thread>
#include <chrono>

int main(int argc, char** argv) {
    unsigned short port = 4242;
    if (argc > 1) port = static_cast<unsigned short>(std::stoi(argv[1]));
    std::cout << "Starting r-type_server on UDP port " << port << "...\n";
    rtype::server::UdpServer server(port);
    server.start();
    std::this_thread::sleep_for(std::chrono::hours(24));
    return 0;
}
