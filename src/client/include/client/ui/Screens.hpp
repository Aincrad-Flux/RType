#pragma once
#include <string>
#include <vector>
#include <cstdint>


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
    NotEnoughPlayers,
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
    void drawNotEnoughPlayers(ScreenState& screen);
    static void logMessage(const std::string& msg, const char* level = "INFO");
    // Gracefully leave any active multiplayer session (sends Disconnect, closes socket)
    void leaveSession();
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
    void sendDisconnect();
    void sendInput(std::uint8_t bits);
    void pumpNetworkOnce();
    struct PackedEntity { unsigned id; unsigned char type; float x; float y; float vx; float vy; unsigned rgba; };
    std::vector<PackedEntity> _entities;
    double _lastSend = 0.0;
    bool _serverReturnToMenu = false;
};

} // namespace ui
} // namespace client
