#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include <unordered_map>
#include <memory>
#include <raylib.h>
#include <utility>
#include <random>

// ECS Engine (standalone) headers for local singleplayer test
#include "rt/ecs/Registry.hpp"
#include "rt/components/Position.hpp"
#include "rt/components/Velocity.hpp"
#include "rt/components/Controller.hpp"
#include "rt/components/Player.hpp"
#include "rt/components/Size.hpp"
#include "rt/components/Collided.hpp"
#include "rt/components/AiController.hpp"
#include "rt/components/Enemy.hpp"
#include "rt/systems/PlayerControlSystem.hpp"
#include "rt/systems/MovementSystem.hpp"
#include "rt/systems/AiControlSystem.hpp"
#include "rt/systems/CollisionSystem.hpp"

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
    // --- Local Singleplayer test (engine sandbox) ---
    void initSingleplayerWorld();
    void shutdownSingleplayerWorld();
    void updateSingleplayerWorld(float dt);
    void drawSingleplayerWorld();
    // Formation wave spawners (singleplayer sandbox)
    void spSpawnLine(int count, float y);
    void spSpawnSnake(int count, float y, float amplitude, float frequency, float spacing);
    void spSpawnTriangle(int rows, float y, float spacing);
    void spSpawnDiamond(int rows, float y, float spacing);
    void spScheduleNextSpawn();
    bool _singleplayerActive = false;
    bool _spPaused = false;
    std::unique_ptr<rt::ecs::Registry> _spWorld;
    rt::ecs::Entity _spPlayer = 0;
    enum class SpFormationKind { Line, Snake, Triangle, Diamond };
    struct SpEnemy {
        rt::ecs::Entity id{0};
        SpFormationKind kind{SpFormationKind::Line};
        int index{0};
        float baseY{0.f};
        float spacing{36.f};
        float amplitude{60.f};
        float frequency{2.0f};
        float spawnTime{0.f};
        float localX{0.f};
        float localY{0.f};
    };
    std::vector<SpEnemy> _spEnemies;
    struct SpBullet { float x; float y; float vx; float vy; float w; float h; };
    std::vector<SpBullet> _spBullets;
    float _spElapsed = 0.f;
    float _spSpawnTimer = 0.f;
    int _spNextFormation = 0;
    std::mt19937 _spRng{};
    float _spNextSpawnDelay = 2.0f;
    float _spMinSpawnDelay = 1.8f;
    float _spMaxSpawnDelay = 3.6f;
    std::size_t _spEnemyCap = 40;
    float _spShootCooldown = 0.f;
    float _spShootInterval = 0.18f;
    float _spBulletSpeed = 420.f;
    float _spBulletW = 8.f;
    float _spBulletH = 3.f;
    float _spHitIframes = 1.f;
    float _spHitIframesDuration = 1.0f;
    float _spHeat = 1.0f;
    float _spHeatDrainPerSec = 0.30f;
    float _spHeatRegenPerSec = 0.15f;
    bool _spInitialized = false;

    // --- Networking ---
    bool assetsAvailable() const;
    void handleNetPacket(const char* data, std::size_t n);
    int _focusedField = 0;
    std::string _statusMessage;
    bool _connected = false;
    std::string _username;
    std::string _serverAddr;
    std::string _serverPort;
    void ensureNetSetup();
    void teardownNet();
    void sendDisconnect();
    void sendInput(std::uint8_t bits);
    void pumpNetworkOnce();
    bool waitHelloAck(double timeoutSec);
    struct PackedEntity { unsigned id; unsigned char type; float x; float y; float vx; float vy; unsigned rgba; };
    std::vector<PackedEntity> _entities;
    double _lastSend = 0.0;
    bool _serverReturnToMenu = false;

    // --- Packet sequence counter for ClientPackage ---
    std::uint32_t _sequenceCounter = 0; // increments each time sendInput() is called

    // --- Sprites ---
    void loadSprites();
    void loadEnemySprites();
    std::string findSpritePath(const char* name) const;
    Texture2D _sheet{};
    bool _sheetLoaded = false;
    int _sheetCols = 5;
    int _sheetRows = 5;
    float _frameW = 0.f;
    float _frameH = 0.f;
    Texture2D _enemySheet{};
    bool _enemyLoaded = false;
    int _enemyCols = 7;
    int _enemyRows = 3;
    float _enemyFrameW = 0.f;
    float _enemyFrameH = 0.f;
    std::unordered_map<unsigned, int> _spriteRowById;
    int _nextSpriteRow = 0;

    // --- HUD ---
    int _playerLives = 4;
    int _maxLives = 6;
    unsigned _selfId = 0;
    int _score = 0;
    int _level = 1;
    struct OtherPlayer { std::uint32_t id; std::string name; int lives; };
    std::vector<OtherPlayer> _otherPlayers;
    std::uint32_t _localPlayerId = 0;
    bool _haveLocalId = false;
    bool _gameOver = false;

    // --- Client-side charge beam ---
    bool _isCharging = false;
    double _chargeStart = 0.0;
    bool _beamActive = false;
    double _beamEndTime = 0.0;
    float _beamX = 0.0f;
    float _beamY = 0.0f;
    float _beamThickness = 0.0f;

    // --- Shot mode toggle ---
    enum class ShotMode { Normal = 0, Charge = 1 };
    ShotMode _shotMode = ShotMode::Normal;
};

} // namespace ui
} // namespace client
