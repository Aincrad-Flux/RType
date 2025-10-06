#include "UdpServer.hpp"
#include <iostream>
#include <csignal>
#include <thread>
#include <chrono>
#include <asio.hpp>
#include <string>

int main(int argc, char** argv) {
    unsigned short port = 4242;
    if (argc > 1) port = static_cast<unsigned short>(std::stoi(argv[1]));

    // Resolve a primary IPv4 to display (prefer 127.0.0.1 if available)
    std::string displayIp = "0.0.0.0";
    try {
        asio::io_context io;
        asio::ip::udp::resolver res(io);
        auto host = asio::ip::host_name();
        auto results = res.resolve(asio::ip::udp::v4(), host, "");
        std::string fallback;
        for (const auto& e : results) {
            auto ip = e.endpoint().address().to_string();
            if (ip == "127.0.0.1") { displayIp = ip; break; }
            if (fallback.empty()) fallback = ip;
        }
        if (displayIp == "0.0.0.0" && !fallback.empty()) displayIp = fallback;
    } catch (...) {}

    std::cout << "###########################\n";
    std::cout << "Server Started\n";
    std::cout << "IP : " << displayIp << "\n";
    std::cout << "PORT : " << port << "\n";
    std::cout << "###########################\n";

    rtype::server::UdpServer server(port);
    server.start();
    std::cout << "Starting r-type_server on UDP port " << port << "... started\n";
    std::this_thread::sleep_for(std::chrono::hours(24));
    return 0;
}
