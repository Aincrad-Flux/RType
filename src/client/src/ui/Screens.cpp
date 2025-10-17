#include "../../include/client/ui/Screens.hpp"
#include "../../include/client/ui/Widgets.hpp"
#include <raylib.h>
#include <asio.hpp>
#include <memory>
#include <iostream>
#include <vector>
#include <algorithm>
#include <cstring>
#include <algorithm>
#include <thread>
#include <chrono>
#include <string>
#include "common/Protocol.hpp"
using namespace client::ui;

namespace {
    struct UdpClientGlobals {
        std::unique_ptr<asio::io_context> io;
        std::unique_ptr<asio::ip::udp::socket> sock;
        asio::ip::udp::endpoint server;
    } g;
    // Scale factor for enemy visual hitbox (only affects the red outline)
    static constexpr float kEnemyHitboxScale = 1.25f; // 25% larger than sprite bounds
    // Server-side AABB for small enemies (EntityType 2 default)
    static constexpr float kEnemyAabbW = 27.0f;
    static constexpr float kEnemyAabbH = 18.0f;
}

void Screens::leaveSession() {
    // Gracefully leave any active multiplayer session
    teardownNet();
    _connected = false;
    _entities.clear();
    _serverReturnToMenu = false;
}

bool Screens::assetsAvailable() const {
    // Try to locate both player and enemy spritesheets
    std::string p1 = findSpritePath("r-typesheet42.gif");
    std::string p2 = findSpritePath("r-typesheet19.gif");
    return !p1.empty() && !p2.empty();
}

static int baseFontFromHeight(int h) {
    int baseFont = (int)(h * 0.045f);
    if (baseFont < 16) baseFont = 16;
    return baseFont;
}

// Logger utilitaire
void Screens::logMessage(const std::string& msg, const char* level) {
    if (level)
        std::cout << "[" << level << "] " << msg << std::endl;
    else
        std::cout << "[INFO] " << msg << std::endl;
}

// --- Spritesheet helpers ---
std::string Screens::findSpritePath(const char* name) const {
    std::vector<std::string> candidates;
    candidates.emplace_back(std::string("sprites/") + name);
    candidates.emplace_back(std::string("../../sprites/") + name);
    candidates.emplace_back(std::string("../sprites/") + name);
    candidates.emplace_back(std::string("../../../sprites/") + name);
    for (const auto& c : candidates) {
        if (FileExists(c.c_str())) return c;
    }
    return {};
}

void Screens::loadSprites() {
    if (_sheetLoaded) return;
    std::string path = findSpritePath("r-typesheet42.gif");
    if (path.empty()) {
        logMessage("Spritesheet r-typesheet42.gif not found.", "WARN");
        return;
    }
    _sheet = LoadTexture(path.c_str());
    if (_sheet.id == 0) {
        logMessage("Failed to load spritesheet texture.", "ERROR");
        return;
    }
    _sheetLoaded = true;
    _frameW = (float)_sheet.width / (float)_sheetCols;
    _frameH = (float)_sheet.height / (float)_sheetRows;
    logMessage("Spritesheet loaded: " + std::to_string(_sheet.width) + "x" + std::to_string(_sheet.height) +
               ", frame " + std::to_string((int)_frameW) + "x" + std::to_string((int)_frameH), "INFO");
}

void Screens::loadEnemySprites() {
    if (_enemyLoaded) return;
    std::string path = findSpritePath("r-typesheet19.gif");
    if (path.empty()) {
        logMessage("Enemy spritesheet r-typesheet19.gif not found.", "WARN");
        return;
    }
    _enemySheet = LoadTexture(path.c_str());
    if (_enemySheet.id == 0) {
        logMessage("Failed to load enemy spritesheet texture.", "ERROR");
        return;
    }
    _enemyLoaded = true;
    // Per user spec: r-typesheet19.gif (230x97) is a 7x3 grid
    _enemyCols = 7;
    _enemyRows = 3;

    _enemyFrameW = (float)_enemySheet.width / (float)_enemyCols;
    _enemyFrameH = (float)_enemySheet.height / (float)_enemyRows;
    logMessage(
        "Enemy sheet loaded: " + std::to_string(_enemySheet.width) + "x" + std::to_string(_enemySheet.height) +
        ", grid " + std::to_string(_enemyCols) + "x" + std::to_string(_enemyRows) +
        ", frame " + std::to_string((int)_enemyFrameW) + "x" + std::to_string((int)_enemyFrameH), "INFO");
}

Screens::~Screens() {
    // Destructor: textures should already be unloaded via unloadGraphics() before window closes.
    // As a safety net, try to unload if the window is still valid.
    if (IsWindowReady()) {
        if (_sheetLoaded) { UnloadTexture(_sheet); _sheetLoaded = false; }
        if (_enemyLoaded) { UnloadTexture(_enemySheet); _enemyLoaded = false; }
    }
}

void Screens::unloadGraphics() {
    if (_sheetLoaded) {
        UnloadTexture(_sheet);
        _sheetLoaded = false;
    }
    if (_enemyLoaded) {
        UnloadTexture(_enemySheet);
        _enemyLoaded = false;
    }
}

void Screens::drawMenu(ScreenState& screen) {
    int w = GetScreenWidth();
    int h = GetScreenHeight();
    int baseFont = baseFontFromHeight(h);
    titleCentered("R-Type", (int)(h * 0.12f), (int)(h * 0.10f), RAYWHITE);
    int btnWidth = (int)(w * 0.28f);
    int btnHeight = (int)(h * 0.08f);
    int gap = (int)(h * 0.02f);
    int startY = (int)(h * 0.30f);
    int x = (w - btnWidth) / 2;

    // Disabled button styling (greyed out, no hover effect)
    Color disText = DARKGRAY;
    Color disBg = (Color){70, 70, 70, 255};
    Color disHover = disBg;

    // Singleplayer (disabled)
    button({(float)x, (float)startY, (float)btnWidth, (float)btnHeight}, "Singleplayer", baseFont, disText, disBg, disHover);

    // Multiplayer (enabled)
    if (button({(float)x, (float)(startY + (btnHeight + gap) * 1), (float)btnWidth, (float)btnHeight}, "Multiplayer", baseFont, BLACK, LIGHTGRAY, GRAY)) {
        screen = ScreenState::Multiplayer;
        _focusedField = 0;
    }

    // Options (disabled)
    button({(float)x, (float)(startY + (btnHeight + gap) * 2), (float)btnWidth, (float)btnHeight}, "Options", baseFont, disText, disBg, disHover);

    // Leaderboard (disabled)
    button({(float)x, (float)(startY + (btnHeight + gap) * 3), (float)btnWidth, (float)btnHeight}, "Leaderboard", baseFont, disText, disBg, disHover);

    // Quit (enabled)
    if (button({(float)x, (float)(startY + (btnHeight + gap) * 4), (float)btnWidth, (float)btnHeight}, "Quit", baseFont, BLACK, (Color){200, 80, 80, 255}, (Color){230, 120, 120, 255})) {
        screen = ScreenState::Exiting;
    }
}

void Screens::drawSingleplayer(ScreenState& screen, SingleplayerForm& form) {
    int w = GetScreenWidth();
    int h = GetScreenHeight();
    int baseFont = baseFontFromHeight(h);
    titleCentered("Singleplayer", (int)(h * 0.10f), (int)(h * 0.08f), RAYWHITE);

    int formWidth = (int)(w * 0.60f);
    int boxHeight = (int)(h * 0.08f);
    int gapY = (int)(h * 0.05f);
    int startY = (int)(h * 0.28f);
    int x = (w - formWidth) / 2;

    Rectangle userBox = {(float)x, (float)startY, (float)formWidth, (float)boxHeight};
    if (inputBox(userBox, "Username", form.username, _focusedField == 0, baseFont, RAYWHITE, (Color){30, 30, 30, 200}, GRAY, false)) _focusedField = 0;

    int btnWidth = (int)(w * 0.20f);
    int btnHeight = (int)(h * 0.08f);
    int btnY = (int)(userBox.y + userBox.height + gapY);
    int btnGap = (int)(w * 0.02f);
    int btnX = (w - (btnWidth * 2 + btnGap)) / 2;
    bool canStart = !form.username.empty();
    Color startBg = canStart ? (Color){120, 200, 120, 255} : (Color){80, 120, 80, 255};
    Color startHover = canStart ? (Color){150, 230, 150, 255} : (Color){90, 140, 90, 255};
    if (button({(float)btnX, (float)btnY, (float)btnWidth, (float)btnHeight}, "Start", baseFont, BLACK, startBg, startHover)) {
        if (canStart) {
            TraceLog(LOG_INFO, "Starting singleplayer as %s", form.username.c_str());
            screen = ScreenState::Menu;
        }
    }
    if (button({(float)(btnX + btnWidth + btnGap), (float)btnY, (float)btnWidth, (float)btnHeight}, "Back", baseFont, BLACK, LIGHTGRAY, GRAY)) {
        screen = ScreenState::Menu;
    }
}

void Screens::drawMultiplayer(ScreenState& screen, MultiplayerForm& form) {
    int w = GetScreenWidth();
    int h = GetScreenHeight();
    int baseFont = baseFontFromHeight(h);
    titleCentered("Multiplayer", (int)(h * 0.10f), (int)(h * 0.08f), RAYWHITE);
    int formWidth = (int)(w * 0.60f);
    int boxHeight = (int)(h * 0.08f);
    int gapY = (int)(h * 0.06f);
    int startY = (int)(h * 0.28f);
    int x = (w - formWidth) / 2;

    Rectangle userBox = {(float)x, (float)startY, (float)formWidth, (float)boxHeight};
    Rectangle addrBox = {(float)x, (float)(startY + (boxHeight + gapY)), (float)formWidth, (float)boxHeight};
    Rectangle portBox = {(float)x, (float)(startY + 2 * (boxHeight + gapY)), (float)formWidth, (float)boxHeight};

    if (inputBox(userBox, "Username", form.username, _focusedField == 0, baseFont, RAYWHITE, (Color){30, 30, 30, 200}, GRAY, false)) _focusedField = 0;
    if (inputBox(addrBox, "Server address", form.serverAddress, _focusedField == 1, baseFont, RAYWHITE, (Color){30, 30, 30, 200}, GRAY, false)) _focusedField = 1;
    if (inputBox(portBox, "Port", form.serverPort, _focusedField == 2, baseFont, RAYWHITE, (Color){30, 30, 30, 200}, GRAY, true)) _focusedField = 2;

    if (IsKeyPressed(KEY_TAB)) _focusedField = (_focusedField + 1) % 3;

    int btnWidth = (int)(w * 0.20f);
    int btnHeight = (int)(h * 0.08f);
    int btnY = (int)(portBox.y + portBox.height + gapY);
    int btnGap = (int)(w * 0.02f);
    int btnX = (w - (btnWidth * 2 + btnGap)) / 2;
    bool canConnect = !form.username.empty() && !form.serverAddress.empty() && !form.serverPort.empty();
    Color connectBg = canConnect ? (Color){120, 200, 120, 255} : (Color){80, 120, 80, 255};
    Color connectHover = canConnect ? (Color){150, 230, 150, 255} : (Color){90, 140, 90, 255};
    if (button({(float)btnX, (float)btnY, (float)btnWidth, (float)btnHeight}, "Connect", baseFont, BLACK, connectBg, connectHover)) {
        if (canConnect) {
            logMessage("Connecting to " + form.serverAddress + ":" + form.serverPort + " as " + form.username, "INFO");
            try {
                // Store connection params
                _username = form.username;
                _serverAddr = form.serverAddress;
                _serverPort = form.serverPort;
                // Reset HUD/network-related state before connecting
                _selfId = 0;
                _playerLives = 4; // will be updated by Roster/LivesUpdate
                _gameOver = false;
                _otherPlayers.clear();

                // Ensure a persistent UDP socket is created and send Hello once
                teardownNet();
                ensureNetSetup();

                // Wait briefly for HelloAck on the same socket
                double start = GetTime();
                bool ok = false;
                while (GetTime() - start < 1.0) {
                    asio::ip::udp::endpoint from;
                    std::array<char, 1024> in{};
                    asio::error_code ec;
                    std::size_t n = g.sock->receive_from(asio::buffer(in), from, 0, ec);
                    if (!ec && n >= sizeof(rtype::net::Header)) {
                        // Process any message so we don't drop roster/state arriving before HelloAck
                        auto* rh = reinterpret_cast<rtype::net::Header*>(in.data());
                        if (rh->version == rtype::net::ProtocolVersion) {
                            if (rh->type == rtype::net::MsgType::HelloAck) {
                                ok = true;
                                break;
                            }
                            // Feed into normal packet handler (Roster, ScoreUpdate, etc.)
                            handleNetPacket(in.data(), n);
                        }
                    } else if (ec && ec != asio::error::would_block) {
                        logMessage(std::string("Receive error: ") + ec.message(), "ERROR");
                    }
                    // Avoid busy-waiting to keep UI smooth
                    std::this_thread::sleep_for(std::chrono::milliseconds(5));
                }
                if (ok) {
                    _statusMessage = std::string("Player Connected.");
                    _connected = true;
                    screen = ScreenState::Waiting;
                } else {
                    _statusMessage = std::string("Connection failed.");
                    teardownNet();
                }
            } catch (const std::exception& e) {
                logMessage(std::string("Exception: ") + e.what(), "ERROR");
                _statusMessage = std::string("Error: ") + e.what();
                teardownNet();
            }
        }
    }
    if (button({(float)(btnX + btnWidth + btnGap), (float)btnY, (float)btnWidth, (float)btnHeight}, "Back", baseFont, BLACK, LIGHTGRAY, GRAY)) {
        screen = ScreenState::Menu;
    }
    if (!_statusMessage.empty()) {
        titleCentered(_statusMessage.c_str(), (int)(h * 0.85f), baseFont, RAYWHITE);
    }
}

void Screens::drawOptions() {
    int h = GetScreenHeight();
    int baseFont = baseFontFromHeight(h);
    titleCentered("Options", (int)(h * 0.10f), (int)(h * 0.08f), RAYWHITE);
    titleCentered("Coming soon... Press ESC to go back.", (int)(h * 0.50f), baseFont, RAYWHITE);
}

void Screens::drawLeaderboard() {
    int h = GetScreenHeight();
    int baseFont = baseFontFromHeight(h);
    titleCentered("Leaderboard", (int)(h * 0.10f), (int)(h * 0.08f), RAYWHITE);
    titleCentered("Coming soon... Press ESC to go back.", (int)(h * 0.50f), baseFont, RAYWHITE);
}

void Screens::drawNotEnoughPlayers(ScreenState& screen) {
    int w = GetScreenWidth();
    int h = GetScreenHeight();
    int baseFont = baseFontFromHeight(h);

    titleCentered("Not enough players connected", (int)(h * 0.30f), (int)(h * 0.09f), RAYWHITE);
    titleCentered("Another player disconnected. You have been returned from the game.", (int)(h * 0.42f), baseFont, LIGHTGRAY);

    int btnWidth = (int)(w * 0.24f);
    int btnHeight = (int)(h * 0.09f);
    int x = (w - btnWidth) / 2;
    int y = (int)(h * 0.60f);
    if (button({(float)x, (float)y, (float)btnWidth, (float)btnHeight}, "Back to Menu", baseFont, BLACK, LIGHTGRAY, GRAY)) {
        screen = ScreenState::Menu;
    }
}

// --- Minimal gameplay networking and rendering ---

void Screens::ensureNetSetup() {
    if (g.io) return;
    g.io = std::make_unique<asio::io_context>();
    g.sock = std::make_unique<asio::ip::udp::socket>(*g.io);
    g.sock->open(asio::ip::udp::v4());
    asio::ip::udp::resolver resolver(*g.io);
    g.server = *resolver.resolve(asio::ip::udp::v4(), _serverAddr, _serverPort).begin();
    g.sock->non_blocking(true);
    _serverReturnToMenu = false;
    // Re-send Hello on gameplay entry so server registers this endpoint for state
    rtype::net::Header hdr{};
    hdr.version = rtype::net::ProtocolVersion;
    hdr.type = rtype::net::MsgType::Hello;
    hdr.size = static_cast<std::uint16_t>(_username.size());
    std::vector<char> out(sizeof(rtype::net::Header) + _username.size());
    std::memcpy(out.data(), &hdr, sizeof(hdr));
    if (!_username.empty()) std::memcpy(out.data() + sizeof(hdr), _username.data(), _username.size());
    g.sock->send_to(asio::buffer(out), g.server);
}

void Screens::sendDisconnect() {
    if (!g.sock) return;
    rtype::net::Header hdr{};
    hdr.version = rtype::net::ProtocolVersion;
    hdr.type = rtype::net::MsgType::Disconnect;
    hdr.size = 0;
    std::array<char, sizeof(rtype::net::Header)> buf{};
    std::memcpy(buf.data(), &hdr, sizeof(hdr));
    asio::error_code ec;
    g.sock->send_to(asio::buffer(buf), g.server, 0, ec);
}

void Screens::teardownNet() {
    if (g.sock && g.sock->is_open()) {
        // Best-effort notify server that we intentionally disconnect
        sendDisconnect();
        asio::error_code ec; g.sock->close(ec);
    }
    g.sock.reset();
    g.io.reset();
    // Reset sprite assignments when disconnecting
    _spriteRowById.clear();
    _nextSpriteRow = 0;
}

void Screens::sendInput(std::uint8_t bits) {
    if (!g.sock) return;

    // --- Build the new ClientPackage correctly aligned ---
    rtype::net::ClientPackage pkg{};
    pkg.sequence    = ++_sequenceCounter;      // Each input tick increases sequence
    pkg.inputBits   = bits;                    // All movement/shoot bits
    pkg.actionFlags = 0;                       // Reserved for now
    pkg.chargeLevel = 0.f;
    pkg.pingTime    = 0;

    // Handle charging info (optional visual feedback / weapon)
    if (_isCharging) {
        double now = GetTime();
        pkg.chargeLevel = static_cast<float>(std::min(2.0, now - _chargeStart));
    }

    // --- Build packet header ---
    rtype::net::Header hdr{};
    hdr.version = rtype::net::ProtocolVersion;
    hdr.type    = rtype::net::MsgType::Input;
    hdr.size    = static_cast<std::uint16_t>(sizeof(pkg));

    // --- Serialize header + package into one buffer ---
    std::array<char, sizeof(hdr) + sizeof(pkg)> buf{};
    std::memcpy(buf.data(), &hdr, sizeof(hdr));
    std::memcpy(buf.data() + sizeof(hdr), &pkg, sizeof(pkg));

    // --- Send to server ---
    asio::error_code ec;
    g.sock->send_to(asio::buffer(buf), g.server, 0, ec);
    if (ec) {
        logMessage(std::string("Send error: ") + ec.message(), "ERROR");
    }
}

void Screens::pumpNetworkOnce() {
    if (!g.sock) return;
    // Drain a small batch each frame to avoid backlog bursts
    for (int i = 0; i < 8; ++i) {
        asio::ip::udp::endpoint from;
        std::array<char, 8192> in{};
        asio::error_code ec;
        std::size_t n = g.sock->receive_from(asio::buffer(in), from, 0, ec);
        if (ec == asio::error::would_block) break;
        if (ec || n < sizeof(rtype::net::Header)) break;
        handleNetPacket(in.data(), n);
    }
}

void Screens::handleNetPacket(const char* data, std::size_t n) {
    if (!data || n < sizeof(rtype::net::Header)) return;
    const auto* h = reinterpret_cast<const rtype::net::Header*>(data);
    if (h->version != rtype::net::ProtocolVersion) return;
    if (h->type == rtype::net::MsgType::State) {
        const char* p = data + sizeof(rtype::net::Header);
        if (n < sizeof(rtype::net::Header) + sizeof(rtype::net::StateHeader)) return;
        auto* sh = reinterpret_cast<const rtype::net::StateHeader*>(p);
        p += sizeof(rtype::net::StateHeader);
        std::size_t count = sh->count;
        if (n < sizeof(rtype::net::Header) + sizeof(rtype::net::StateHeader) + count * sizeof(rtype::net::PackedEntity)) return;
        _entities.clear();
        _entities.reserve(count);
        auto* arr = reinterpret_cast<const rtype::net::PackedEntity*>(p);
        for (std::size_t i = 0; i < count; ++i) {
            PackedEntity e{};
            e.id = arr[i].id;
            e.type = static_cast<unsigned char>(arr[i].type);
            e.x = arr[i].x; e.y = arr[i].y; e.vx = arr[i].vx; e.vy = arr[i].vy;
            e.rgba = arr[i].rgba;
            _entities.push_back(e);
        }
    } else if (h->type == rtype::net::MsgType::Roster) {
        const char* p = data + sizeof(rtype::net::Header);
        if (n < sizeof(rtype::net::Header) + sizeof(rtype::net::RosterHeader)) return;
        auto* rh = reinterpret_cast<const rtype::net::RosterHeader*>(p);
        p += sizeof(rtype::net::RosterHeader);
        std::size_t count = rh->count;
        if (n < sizeof(rtype::net::Header) + sizeof(rtype::net::RosterHeader) + count * sizeof(rtype::net::PlayerEntry)) return;
        _otherPlayers.clear();
        // Determine truncated username as stored by server (15 chars max)
        std::string unameTrunc = _username.substr(0, 15);
        for (std::size_t i = 0; i < count; ++i) {
            auto* pe = reinterpret_cast<const rtype::net::PlayerEntry*>(p + i * sizeof(rtype::net::PlayerEntry));
            std::string name(pe->name, pe->name + strnlen(pe->name, sizeof(pe->name)));
            int lives = std::clamp<int>(pe->lives, 0, 10);
            if (name == unameTrunc) {
                _playerLives = lives;
                _selfId = pe->id;
                continue; // don't include self in top bar list
            }
            _otherPlayers.push_back({pe->id, name, lives});
        }
        // Keep at most 3 teammates in top bar
        if (_otherPlayers.size() > 3)
            _otherPlayers.resize(3);
    } else if (h->type == rtype::net::MsgType::LivesUpdate) {
        const char* p = data + sizeof(rtype::net::Header);
        if (n < sizeof(rtype::net::Header) + sizeof(rtype::net::LivesUpdatePayload)) return;
        auto* lu = reinterpret_cast<const rtype::net::LivesUpdatePayload*>(p);
        unsigned id = lu->id;
        int lives = std::clamp<int>(lu->lives, 0, 10);
        if (id == _selfId) {
            _playerLives = lives;
            _gameOver = (_playerLives <= 0);
        } else {
            for (auto& op : _otherPlayers) {
                if (op.id == id) { op.lives = lives; break; }
            }
        }
    } else if (h->type == rtype::net::MsgType::ScoreUpdate) {
        const char* p = data + sizeof(rtype::net::Header);
        if (n < sizeof(rtype::net::Header) + sizeof(rtype::net::ScoreUpdatePayload)) return;
        auto* su = reinterpret_cast<const rtype::net::ScoreUpdatePayload*>(p);
        // Team-wide score
        _score = su->score;
    } else if (h->type == rtype::net::MsgType::ReturnToMenu) {
        _serverReturnToMenu = true;
    } else {
        // ignore unknown types
    }
}

void Screens::drawWaiting(ScreenState& screen) {
    int w = GetScreenWidth();
    int h = GetScreenHeight();
    int baseFont = baseFontFromHeight(h);

    // Ensure socket is ready (in case we came here directly)
    ensureNetSetup();

    // Pump one network packet if available to update entities
    pumpNetworkOnce();

    if (_serverReturnToMenu) {
        // Server asked us to return: cleanly leave and show info screen
        leaveSession();
        screen = ScreenState::NotEnoughPlayers;
        return;
    }

    // Count players in the latest world snapshot
    int playerCount = 0;
    for (const auto& e : _entities) {
        if (e.type == 1)
            ++playerCount;
    }

    // UI
    titleCentered("Waiting for players...", (int)(h * 0.25f), (int)(h * 0.08f), RAYWHITE);
    std::string sub = "Players connected: " + std::to_string(playerCount) + "/2";
    titleCentered(sub.c_str(), (int)(h * 0.40f), baseFont, RAYWHITE);

    // Simple animated dots
    int dots = ((int)(GetTime() * 2)) % 4;
    std::string hint = "The game will start automatically" + std::string(dots, '.');
    titleCentered(hint.c_str(), (int)(h * 0.50f), baseFont, LIGHTGRAY);

    // Cancel button
    int btnWidth = (int)(w * 0.18f);
    int btnHeight = (int)(h * 0.08f);
    int x = (w - btnWidth) / 2;
    int y = (int)(h * 0.70f);
    if (button({(float)x, (float)y, (float)btnWidth, (float)btnHeight}, "Cancel", baseFont, BLACK, LIGHTGRAY, GRAY)) {
        teardownNet();
        _connected = false;
        _entities.clear();
        screen = ScreenState::Menu;
        return;
    }

    // Transition to gameplay when at least 2 players are present and assets exist
    if (playerCount >= 2) {
        if (assetsAvailable()) {
            screen = ScreenState::Gameplay;
        } else {
            titleCentered("Missing spritesheet assets. Place sprites/ and try again.", (int)(h * 0.80f), baseFont, RED);
        }
    }
}

void Screens::drawGameplay(ScreenState& screen) {
    // Do not run gameplay if assets are missing; show message and bounce back to menu on ESC
    if (!assetsAvailable()) {
        int h = GetScreenHeight();
        int baseFont = baseFontFromHeight(h);
        titleCentered("Spritesheets not found.", (int)(h * 0.40f), (int)(h * 0.08f), RED);
        titleCentered("Place the sprites/ folder next to the executable then press ESC.", (int)(h * 0.52f), baseFont, RAYWHITE);
        if (IsKeyPressed(KEY_ESCAPE)) {
            leaveSession();
            screen = ScreenState::Menu;
        }
        return;
    }
    if (!_connected) {
        titleCentered("Not connected. Press ESC.", GetScreenHeight()*0.5f, 24, RAYWHITE);
        return;
    }
    ensureNetSetup();

    pumpNetworkOnce();

    if (_serverReturnToMenu) {
        // Server asked us to return: cleanly leave and show info screen
        leaveSession();
        screen = ScreenState::NotEnoughPlayers;
        return;
    }

    pumpNetworkOnce();

    if (_serverReturnToMenu) {
        // Server asked us to return: cleanly leave and show info screen
        leaveSession();
        screen = ScreenState::NotEnoughPlayers;
        return;
    }

    // Lazy-load spritesheet
    if (!_sheetLoaded) loadSprites();
    if (!_enemyLoaded) loadEnemySprites();

    // Update world snapshot first so we can gate inputs using current position
    pumpNetworkOnce();

    // Gate player inputs so the player cannot leave the playable area (between top and bottom bars)
    int gw = GetScreenWidth();
    int gh = GetScreenHeight();
    int ghudFont = (int)(gh * 0.045f); if (ghudFont < 16) ghudFont = 16;
    int gmargin = 16;
    int gTopReserved = gmargin + ghudFont + gmargin;
    int gBottomBarH = std::max((int)(gh * 0.10f), ghudFont + gmargin);
    int gPlayableMinY = gTopReserved;
    int gPlayableMaxY = gh - gBottomBarH;
    if (gPlayableMaxY < gPlayableMinY + 1) gPlayableMaxY = gPlayableMinY + 1;
    // Find self entity
    const PackedEntity* self = nullptr;
    for (const auto& ent : _entities) {
        if (ent.type == 1 && ent.id == _selfId) { self = &ent; break; }
    }
    // Build input bits with edge gating
    std::uint8_t bits = 0;
    bool wantLeft  = IsKeyDown(KEY_LEFT);
    bool wantRight = IsKeyDown(KEY_RIGHT);
    bool wantUp    = IsKeyDown(KEY_UP);
    bool wantDown  = IsKeyDown(KEY_DOWN);
    bool wantShoot = IsKeyDown(KEY_SPACE);
    if (self) {
        // Estimate drawn size for the player sprite
        const float playerScale = 1.18f;
        float drawW = (_sheetLoaded && _frameW > 0) ? (_frameW * playerScale) : 24.0f;
        float drawH = (_sheetLoaded && _frameH > 0) ? (_frameH * playerScale) : 14.0f;
        const float xOffset = -6.0f; // applied at draw time
        float dstX = self->x + xOffset;
        float dstY = self->y;
        float minX = 0.0f;
        float maxX = (float)gw - drawW;
    float minY = (float)gPlayableMinY;
    float maxY = (float)gPlayableMaxY - drawH;
        if (wantLeft  && dstX > minX) bits |= rtype::net::InputLeft;
        if (wantRight && dstX < maxX) bits |= rtype::net::InputRight;
        if (wantUp    && dstY > minY) bits |= rtype::net::InputUp;
        if (wantDown  && dstY < maxY) bits |= rtype::net::InputDown;
    } else {
        // Fallback: no gating if we don't know self position yet
        if (wantLeft)  bits |= rtype::net::InputLeft;
        if (wantRight) bits |= rtype::net::InputRight;
        if (wantUp)    bits |= rtype::net::InputUp;
        if (wantDown)  bits |= rtype::net::InputDown;
    }
    // Toggle shot mode on Ctrl press (once)
    if (IsKeyPressed(KEY_LEFT_CONTROL) || IsKeyPressed(KEY_RIGHT_CONTROL)) {
        _shotMode = (_shotMode == ShotMode::Normal) ? ShotMode::Charge : ShotMode::Normal;
    }
    // Charge beam handling: hold Space when in Charge mode (Alt no longer required)
    bool altDown = false;
    bool chargeHeld = (_shotMode == ShotMode::Charge) && IsKeyDown(KEY_SPACE);
    if (chargeHeld) {
        if (!_isCharging) { _isCharging = true; _chargeStart = GetTime(); }
    } else {
        if (_isCharging && IsKeyReleased(KEY_SPACE)) {
            // Fire beam
            double chargeDur = std::min(2.0, std::max(0.1, GetTime() - _chargeStart));
            _beamActive = true;
            _beamEndTime = GetTime() + 0.25; // beam visible for 250ms
            // Beam origin at player position
            float px = self ? self->x : 0.0f;
            float py = self ? self->y : (float)((gPlayableMinY + gPlayableMaxY) / 2);
            _beamX = px;
            _beamY = py;
            // Thickness scales with charge duration
            _beamThickness = (float)(8.0 + chargeDur * 22.0); // 8..52 px
        }
        _isCharging = false;
    }
    // Normal shoot bit only when not charging
    if (wantShoot && _shotMode == ShotMode::Normal) bits |= rtype::net::InputShoot;
    // Send charge bit to engine so server handles charge logic when shot mode is Charge
    if (chargeHeld) bits |= rtype::net::InputCharge;

    double now = GetTime();
    if (now - _lastSend > 1.0/30.0) {
        sendInput(bits);
        _lastSend = now;
    }


    // --- HUD (Top other players + Bottom bar for lives/score/level) ---
    int w = GetScreenWidth();
    int h = GetScreenHeight();
    int hudFont = (int)(h * 0.045f);
    if (hudFont < 16) hudFont = 16;
    int margin = 16;

    // Top area: show up to 3 teammates (exclude self), capping lives icons to 3
    int topY = margin;
    int xCursor = margin;
    int shown = 0;
    for (size_t i = 0; i < _otherPlayers.size() && shown < 3; ++i, ++shown) {
        const auto& op = _otherPlayers[i];
        // Name
        DrawText(op.name.c_str(), xCursor, topY, hudFont, RAYWHITE);
        int nameW = MeasureText(op.name.c_str(), hudFont);
        xCursor += nameW + 12;
        // Lives as small red squares (cap to 3)
        int iconSize = std::max(6, hudFont / 2 - 2);
        int iconGap  = std::max(3, iconSize / 3);
        int livesToDraw = std::min(3, std::max(0, op.lives));
        int iconsW = livesToDraw * iconSize + (livesToDraw > 0 ? (livesToDraw - 1) * iconGap : 0);
        int iconY = topY + (hudFont - iconSize) / 2;
        for (int k = 0; k < livesToDraw; ++k) {
            int ix = xCursor + k * (iconSize + iconGap);
            DrawRectangle(ix, iconY, iconSize, iconSize, (Color){220, 80, 80, 255});
        }
        xCursor += iconsW + 24; // spacing to next player
    }
    // If more than 3 teammates, show overflow counter
    if (_otherPlayers.size() > 3) {
        std::string more = "x " + std::to_string(_otherPlayers.size() - 3);
        DrawText(more.c_str(), xCursor, topY, hudFont, LIGHTGRAY);
    }

    // Height reserved by top area (no longer used for clamping player)
    int topReserved = topY + hudFont + margin;

    // Bottom bar: player's lives (left), score (center), level (right)
    int bottomBarH = std::max((int)(h * 0.10f), hudFont + margin);
    int bottomY = h - bottomBarH;
    DrawRectangle(0, bottomY, w, bottomBarH, (Color){20, 20, 20, 200});
    DrawRectangleLines(0, bottomY, w, bottomBarH, (Color){60, 60, 60, 255});

    // Left: lives 0..10
    int iconSize = std::max(10, std::min(18, bottomBarH - margin - 10));
    int iconGap  = std::max(4, iconSize / 3);
    int livesToDraw = std::min(10, std::max(0, _playerLives));
    int iconRowY = bottomY + (bottomBarH - iconSize) / 2;
    int iconRowX = margin;
    for (int i = 0; i < 10; ++i) {
        Color c = (i < livesToDraw) ? (Color){220, 80, 80, 255} : (Color){70, 70, 70, 255};
        DrawRectangle(iconRowX + i * (iconSize + iconGap), iconRowY, iconSize, iconSize, c);
    }

    // Center: Score
    std::string scoreStr = "Score: " + std::to_string(_score);
    int scoreW = MeasureText(scoreStr.c_str(), hudFont);
    int scoreX = (w - scoreW) / 2;
    int textY = bottomY + (bottomBarH - hudFont) / 2;
    DrawText(scoreStr.c_str(), scoreX, textY, hudFont, RAYWHITE);

    // Right: Shot mode instead of Level
    const char* modeStr = (_shotMode == ShotMode::Normal) ? "Shot: Normal" : "Shot: Charge";
    int modeW = MeasureText(modeStr, hudFont);
    int modeX = w - margin - modeW;
    DrawText(modeStr, modeX, textY, hudFont, (Color){200, 200, 80, 255});

    // Playable area bounds for drawing: between top bar and bottom bar
    int playableMinY = topReserved;
    int playableMaxY = h - bottomBarH;
    if (playableMaxY < playableMinY + 1) playableMaxY = playableMinY + 1;

    if (_entities.empty()) {
        titleCentered("Connecting to game...", (int)(GetScreenHeight()*0.5f), 24, RAYWHITE);
    }

    // Render entities using persistent sprite-row assignment per player id
    for (std::size_t i = 0; i < _entities.size(); ++i) {
        auto& e = _entities[i];
        Color c = {(unsigned char)((e.rgba>>24)&0xFF),(unsigned char)((e.rgba>>16)&0xFF),(unsigned char)((e.rgba>>8)&0xFF),(unsigned char)(e.rgba&0xFF)};
        if (e.type == 1) { // Player (on applique les contraintes)
            // Taille du vaisseau pour le fallback rect
            int shipW = 20, shipH = 12;
            // Clamp to full window vertically and horizontally
            if (e.y < playableMinY) e.y = (float)playableMinY;
            if (e.y + shipH > playableMaxY) e.y = (float)(playableMaxY - shipH);
            if (e.x < 0) e.x = 0;
            if (e.x + shipW > w) e.x = (float)(w - shipW);
            // Get or assign a fixed row for this player id
            int rowIndex;
            auto it = _spriteRowById.find(e.id);
            if (it == _spriteRowById.end()) {
                rowIndex = _nextSpriteRow % _sheetRows;
                _spriteRowById[e.id] = rowIndex;
                _nextSpriteRow++;
            } else {
                rowIndex = it->second;
            }
            if (_sheetLoaded && _frameW > 0 && _frameH > 0) {
                int colIndex = 2;
                const float playerScale = 1.18f;
                float drawW = _frameW * playerScale;
                float drawH = _frameH * playerScale;
                // Apply a small left offset and clamp using destination coords
                const float xOffset = -6.0f;
                float dstX = e.x + xOffset;
                float dstY = e.y;
                if (dstY < playableMinY) dstY = (float)playableMinY;
                if (dstX < 0) dstX = 0;
                if (dstX + drawW > w) dstX = (float)(w - drawW);
                if (dstY + drawH > playableMaxY) dstY = (float)(playableMaxY - drawH);
                Rectangle src{ _frameW * colIndex, _frameH * rowIndex, _frameW, _frameH };
                Rectangle dst{ dstX, dstY, drawW, drawH };
                Vector2 origin{ 0.0f, 0.0f };
                DrawTexturePro(_sheet, src, dst, origin, 0.0f, WHITE);
            } else {
                // No fallback debug rectangle: keep player invisible if sprite isn't available
            }
        } else if (e.type == 2) { // Enemy
            if (_enemyLoaded && _enemyFrameW > 0 && _enemyFrameH > 0) {
                // Last row is staggered (quinconce): use fractional column index 3.5 on row 2 (zero-based)
                const float colIndex = 2.5f; // zero-based fractional column
                const int rowIndex = 2;      // zero-based last row
                const float cropPx = 10.0f;
                float srcH = _enemyFrameH - cropPx;
                if (srcH < 1.0f) srcH = 1.0f; // avoid zero/negative height
                // Only draw when the enemy AABB is fully inside the playable area to avoid edge pop-in
                float ex = e.x;
                float ey = e.y;
                if (ey < playableMinY || ey + kEnemyAabbH > playableMaxY) continue;
                if (ex < 0 || ex + kEnemyAabbW > w) continue;
                Rectangle src{ _enemyFrameW * colIndex, _enemyFrameH * rowIndex, _enemyFrameW, srcH };
                Rectangle dst{ ex, ey, kEnemyAabbW, kEnemyAabbH };
                Vector2 origin{ 0.0f, 0.0f };
                DrawTexturePro(_enemySheet, src, dst, origin, 0.0f, WHITE);
                // Removed enemy debug hitbox outline
            } else {
                // No fallback rectangle for enemies to avoid drawing squares
            }
        } else if (e.type == 3) { // Bullet
            DrawRectangle((int)e.x, (int)e.y, 6, 3, c);
        }
    }

    // Draw charged beam if active
    if (_beamActive) {
        if (GetTime() > _beamEndTime) {
            _beamActive = false;
        } else {
            // Beam goes to the right across the playable area
            float bx = _beamX;
            float by = _beamY;
            // Clamp vertical beam center to playable band
            if (by < playableMinY) by = (float)playableMinY;
            if (by > playableMaxY) by = (float)playableMaxY;
            float halfT = _beamThickness * 0.5f;
            float y0 = std::max((float)playableMinY, by - halfT);
            float y1 = std::min((float)playableMaxY, by + halfT);
            if (y1 > y0) {
                // Core
                DrawRectangle((int)bx, (int)y0, w - (int)bx, (int)(y1 - y0), (Color){120, 200, 255, 220});
                // Glow borders
                DrawRectangle((int)bx, (int)(y0 - 4), w - (int)bx, 4, (Color){120, 200, 255, 120});
                DrawRectangle((int)bx, (int)y1, w - (int)bx, 4, (Color){120, 200, 255, 120});
            }
        }
    }

    // Game Over overlay and input to return to menu
    if (_gameOver) {
        DrawRectangle(0, 0, w, h, (Color){0, 0, 0, 180});
        titleCentered("GAME OVER", (int)(h * 0.40f), (int)(h * 0.10f), RED);
        std::string finalScore = "Score: " + std::to_string(_score);
        titleCentered(finalScore.c_str(), (int)(h * 0.52f), (int)(h * 0.06f), RAYWHITE);
        titleCentered("Press ESC to return to menu", (int)(h * 0.62f), (int)(h * 0.04f), LIGHTGRAY);
        if (IsKeyPressed(KEY_ESCAPE)) {
            teardownNet();
            _connected = false;
            _entities.clear();
            _gameOver = false;
            screen = ScreenState::Menu;
            return;
        }
    }
}
