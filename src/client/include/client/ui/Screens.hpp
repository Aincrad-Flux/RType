#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include <unordered_map>
#include <raylib.h>
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
    // Release GPU textures (must be called before closing the window)
    void unloadGraphics();
    ~Screens();
private:
    // Check if required sprite assets are available on disk
    bool assetsAvailable() const;
    // Parse a single UDP datagram payload according to our protocol and update local state
    void handleNetPacket(const char* data, std::size_t n);
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
    void sendDisconnect();
    void sendInput(std::uint8_t bits);
    void pumpNetworkOnce();
    struct PackedEntity { unsigned id; unsigned char type; float x; float y; float vx; float vy; unsigned rgba; };
    std::vector<PackedEntity> _entities;
    double _lastSend = 0.0;
    bool _serverReturnToMenu = false;
    // --- spritesheet handling ---
    void loadSprites();
    void loadEnemySprites();
    std::string findSpritePath(const char* name) const;
    Texture2D _sheet{};
    bool _sheetLoaded = false;
    int _sheetCols = 5; // spritesheet is 5x5 per user spec
    int _sheetRows = 5;
    float _frameW = 0.f;
    float _frameH = 0.f;
    // Enemy spritesheet (r-typesheet19.gif, 230x97)
    Texture2D _enemySheet{};
    bool _enemyLoaded = false;
    int _enemyCols = 7; // per user spec: 7 columns
    int _enemyRows = 3; // per user spec: 3 rows
    float _enemyFrameW = 0.f;
    float _enemyFrameH = 0.f;
    // Fixed sprite assignment per player id
    std::unordered_map<unsigned, int> _spriteRowById; // id -> row index
    int _nextSpriteRow = 0; // next row to assign on first sight

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

    // --- Client-side charge beam (Alt + Space) ---
    bool _isCharging = false;
    double _chargeStart = 0.0;
    bool _beamActive = false;
    double _beamEndTime = 0.0;
    float _beamX = 0.0f;        // origin X at release
    float _beamY = 0.0f;        // center Y at release
    float _beamThickness = 0.0f; // computed from charge duration

    // --- Shot mode toggle (Normal vs Charge), switched with Ctrl key ---
    enum class ShotMode { Normal = 0, Charge = 1 };
    ShotMode _shotMode = ShotMode::Normal;
};

} // namespace ui
} // namespace client
