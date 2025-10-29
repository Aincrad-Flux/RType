#pragma once
#include <asio.hpp>
#include <thread>
#include <array>
#include <unordered_map>
#include <vector>
#include <random>
#include <string>
#include <chrono>
#include <functional>
#include "common/Protocol.hpp"
#include "rt/ecs/Registry.hpp"

// Forward declaration to avoid including heavy headers in the interface
namespace rt { namespace game { class FormationSpawnSystem; } }

namespace rtype::server {
class TcpServer;
}

namespace rtype::server::gameplay {

class GameSession {
public:
    using SendFn = std::function<void(const asio::ip::udp::endpoint&, const void*, std::size_t)>;

    GameSession(asio::io_context& io, SendFn sendFn, rtype::server::TcpServer* tcpServer);
    ~GameSession();

    void start();
    void stop();

    // Handle UDP packet from client
    void onUdpPacket(const asio::ip::udp::endpoint& from, const char* data, std::size_t size);

private:
    void gameLoop();
    void checkTimeouts();
    void removeClient(const std::string& key);
    void broadcastState();
    void broadcastRoster();
    void broadcastLivesUpdate(std::uint32_t id, std::uint8_t lives);
    void broadcastLobbyStatus();
    void maybeStartGame();
    void cleanupGameWorld();

    static std::string makeKey(const asio::ip::udp::endpoint& ep);

private:
    asio::io_context& io_;
    SendFn send_;

    std::thread gameThread_;
    bool running_ = false;

    std::chrono::steady_clock::time_point lastStateSend_{};
    double stateHz_ = 20.0;

    std::unordered_map<std::string, std::uint32_t> endpointToPlayerId_;
    std::unordered_map<std::string, asio::ip::udp::endpoint> keyToEndpoint_;
    std::unordered_map<std::uint32_t, std::uint8_t> playerInputBits_;
    std::unordered_map<std::uint32_t, std::string> playerNames_;
    std::unordered_map<std::uint32_t, std::uint8_t> playerLives_;
    std::unordered_map<std::uint32_t, std::int32_t> playerScores_;
    std::int32_t lastTeamScore_ = 0;

    rt::ecs::Registry reg_;
    std::mt19937 rng_;
    std::unordered_map<std::string, std::chrono::steady_clock::time_point> lastSeen_;

    rtype::server::TcpServer* tcp_ = nullptr;
    bool gameStarted_ = false;

    // Lobby state
    std::uint32_t hostId_ = 0;
    std::uint8_t lobbyBaseLives_ = 4;
    std::uint8_t lobbyDifficulty_ = 1;
};

}

