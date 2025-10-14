#include "UdpServer.hpp"
#include <iostream>
#include <csignal>
#include <thread>
#include <chrono>
#include <asio.hpp>
#include <string>

int main(int argc, char** argv) {
    unsigned short port = 4242;
    if (argc > 1) {
        try {
            int p = std::stoi(argv[1]);
            if (p < 1 || p > 65535) {
                std::cerr << "Invalid port: " << argv[1] << " (must be 1..65535). Using default 4242.\n";
            } else {
                port = static_cast<unsigned short>(p);
            }
        } catch (const std::exception& ex) {
            std::cerr << "Invalid port argument: '" << argv[1] << "' (" << ex.what() << "). Using default 4242.\n";
        }
    }

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

    try {
        rtype::server::UdpServer server(port);
        server.start();
        std::cout << "Starting r-type_server on UDP port " << port << "... started\n";
        std::this_thread::sleep_for(std::chrono::hours(24));
        return 0;
    } catch (const asio::system_error& se) {
        std::error_code ec = se.code();
        std::cerr << "Failed to start server on port " << port << ": " << se.what() << " (code " << ec.value() << ")\n";
        if (ec.value() == EADDRINUSE) {
            std::cerr << "Hint: The port is already in use by another process. Choose a different port or stop the other service.\n";
        }
        return 1;
    } catch (const std::exception& ex) {
        std::cerr << "Failed to start server on port " << port << ": " << ex.what() << "\n";
        return 1;
    }
}
