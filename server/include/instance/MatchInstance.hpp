#pragma once
#include <asio.hpp>
#include <unordered_map>
#include <vector>
#include <string>
#include <random>
#include <thread>
#include <chrono>
#include "common/Protocol.hpp"
#include "gameplay/GameSession.hpp"

namespace rt { namespace game { class FormationSpawnSystem; class PlayerInput; class Transform; class Velocity; class HitFlag; class Invincible; class Score; class NetType; class Formation; } }

namespace rtype::server { namespace net { class NetworkManager; } }

namespace rtype::server::instance {

class MatchInstance {
  public:
    explicit MatchInstance(net::NetworkManager& net);
    ~MatchInstance();
    void start();
    void stop();

    void handlePacket(const asio::ip::udp::endpoint& from, const char* data, std::size_t size);

  private:
    void runLoop();
    void removeClient(const std::string& key);
    void broadcastState();
    void broadcastRoster();
    void broadcastLivesUpdate(std::uint32_t id, std::uint8_t lives);
    void broadcastScoreUpdate(std::uint32_t id, std::int32_t score);
    void broadcastLobbyStatus();
    void broadcastGameOver(std::uint8_t reason = 0);

    // Lobby / match state
    std::uint32_t hostId_ = 0;
    std::uint8_t lobbyBaseLives_ = 4;    // 1..6
    std::uint8_t lobbyDifficulty_ = 1;   // 0=Easy,1=Normal,2=Hard
    std::uint8_t shooterPercent_ = 25;   // percent of enemies that can shoot
    bool matchStarted_ = false;

    // gameplay state
    std::unordered_map<std::string, std::uint32_t> endpointToPlayerId_;
    std::unordered_map<std::uint32_t, std::uint8_t> playerInputBits_;
    std::unordered_map<std::uint32_t, std::string> playerNames_;
    std::unordered_map<std::uint32_t, std::uint8_t> playerLives_; // 0..10
    std::unordered_map<std::uint32_t, std::int32_t> playerScores_;
    std::int32_t lastTeamScore_ = 0; // shared score across all players

    // ECS session
    rtype::server::game::GameSession session_;
    std::mt19937 rng_;
    rt::game::FormationSpawnSystem* spawnSys_ = nullptr;

    // Timing
    std::thread gameThread_;
    bool running_ = false;
    std::chrono::steady_clock::time_point lastStateSend_{};
    double stateHz_ = 20.0; // send world state 20 times per second

    // bridge to networking
    rtype::server::net::NetworkManager& net_;
};

}
