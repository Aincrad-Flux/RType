#pragma once
#include <asio.hpp>
#include <thread>
#include <array>
#include <unordered_map>
#include <vector>
#include <random>
#include <string>
#include <chrono>
#include "common/Protocol.hpp"
#include "rt/ecs/Registry.hpp"

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
    void gameLoop();
    void broadcastState();
  void broadcastRoster();
  void broadcastLivesUpdate(std::uint32_t id, std::uint8_t lives);
  void broadcastScoreUpdate(std::uint32_t id, std::int32_t score);
    void checkTimeouts();
    void removeClient(const std::string& key);

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
  std::unordered_map<std::uint32_t, std::string> playerNames_;
  std::unordered_map<std::uint32_t, std::uint8_t> playerLives_; // 0..10
  std::unordered_map<std::uint32_t, std::int32_t> playerScores_;
  std::chrono::steady_clock::time_point lastRosterSend_{}; // kept if we throttle in future
    // ECS registry holds all entities/components
    rt::ecs::Registry reg_;
    std::mt19937 rng_;
    std::unordered_map<std::string,
    std::chrono::steady_clock::time_point> lastSeen_;
};

}
