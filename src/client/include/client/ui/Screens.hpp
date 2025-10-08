#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include <utility>


namespace client {
namespace ui {

enum class ScreenState {
    Menu,
    Singleplayer,
    Multiplayer,
    Waiting,
    Gameplay,
    Options,
    Leaderboard,
    Exiting
};

struct MultiplayerForm {
    std::string username;
    std::string serverAddress;
    std::string serverPort;
};

struct SingleplayerForm {
    std::string username;
};

class Screens {
public:
    void drawMenu(ScreenState& screen);
    void drawSingleplayer(ScreenState& screen, SingleplayerForm& form);
    void drawMultiplayer(ScreenState& screen, MultiplayerForm& form);
    void drawWaiting(ScreenState& screen);
    void drawGameplay(ScreenState& screen);
    void drawOptions();
    void drawLeaderboard();
    static void logMessage(const std::string& msg, const char* level = "INFO");
private:
    int _focusedField = 0; // 0=user, 1=addr, 2=port
    std::string _statusMessage;
    // network state for gameplay
    bool _connected = false;
    std::string _username;
    std::string _serverAddr;
    std::string _serverPort;
    // lightweight UDP client
    void ensureNetSetup();
    void teardownNet();
    void sendInput(std::uint8_t bits);
    void pumpNetworkOnce();
    struct PackedEntity { unsigned id; unsigned char type; float x; float y; float vx; float vy; unsigned rgba; };
    std::vector<PackedEntity> _entities;
    double _lastSend = 0.0;

    // --- Gameplay HUD state (placeholders until server data is wired) ---
    int _playerLives = 4; // 0..10 (updated via Roster/LivesUpdate)
    unsigned _selfId = 0;  // our player id (from roster)
    int _score = 0;
    int _level = 1;
        struct OtherPlayer { std::uint32_t id; std::string name; int lives; };
        std::vector<OtherPlayer> _otherPlayers; // show up to 3 then "+ x"
        std::uint32_t _localPlayerId = 0; // received from Roster
        bool _haveLocalId = false;
    bool _gameOver = false; // set when our lives reach 0
};

} // namespace ui
} // namespace client
