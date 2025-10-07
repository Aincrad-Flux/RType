#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include <unordered_map>
#include <raylib.h>


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
    ~Screens();
private:
    int _focusedField = 0;
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
    // --- spritesheet handling ---
    void loadSprites();
    std::string findSpritePath(const char* name) const;
    Texture2D _sheet{};
    bool _sheetLoaded = false;
    int _sheetCols = 5;
    int _sheetRows = 5;
    float _frameW = 0.f;
    float _frameH = 0.f;
    // Fixed sprite assignment per player id
    std::unordered_map<unsigned, int> _spriteRowById;
    int _nextSpriteRow = 0;
};

} // namespace ui
} // namespace client
