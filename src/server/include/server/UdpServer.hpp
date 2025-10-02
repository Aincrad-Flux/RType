#pragma once
#include <asio.hpp>
#include <thread>
#include <array>
#include <unordered_map>
#include <string>
#include "common/Protocol.hpp"

namespace rtype::server {

class UdpServer {
  public:
    explicit UdpServer(unsigned short port);
    ~UdpServer();
    void start();
    void stop();
  private:
    void doReceive();
    void doSend(const asio::ip::udp::endpoint& to, const void* data, std::size_t size);
    void handlePacket(const asio::ip::udp::endpoint& from, const char* data, std::size_t size);

    asio::io_context io_;
    asio::ip::udp::socket socket_;
    std::array<char, 2048> buffer_{};
    asio::ip::udp::endpoint remote_;
    std::jthread thread_;
    std::unordered_map<std::string, std::string> endpointToUsername_;
};

}
