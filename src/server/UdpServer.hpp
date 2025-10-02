#pragma once
#include <asio.hpp>
#include <thread>
#include <array>
#include <unordered_map>
#include <vector>
#include <random>
#include <string>
#include "common/Protocol.hpp"

namespace rtype::server {

struct ServerEntity {
    std::uint32_t id;
    rtype::net::EntityType type;
    float x;
    float y;
    float vx;
    float vy;
    std::uint32_t rgba;
};

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
    void gameLoop();
    void broadcastState();

    asio::io_context io_;
    asio::ip::udp::socket socket_;
    std::array<char, 2048> buffer_{};
    asio::ip::udp::endpoint remote_;
    std::thread netThread_;
    std::thread gameThread_;
    bool running_ = false;

    // gameplay state
    std::unordered_map<std::string, std::uint32_t> endpointToPlayerId_;
    std::unordered_map<std::string, asio::ip::udp::endpoint> keyToEndpoint_;
    std::unordered_map<std::uint32_t, std::uint8_t> playerInputBits_;
    std::vector<ServerEntity> entities_;
    std::uint32_t nextEntityId_ = 1;
    std::mt19937 rng_;
};

}
