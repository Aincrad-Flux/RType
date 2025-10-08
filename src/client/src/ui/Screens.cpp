#include "../../include/client/ui/Screens.hpp"
#include "../../include/client/ui/Widgets.hpp"
#include <raylib.h>
#include <asio.hpp>
#include <memory>
#include <iostream>
#include <vector>
#include <cstring>
#include <algorithm>
#include <thread>
#include <chrono>
#include "common/Protocol.hpp"

namespace client {
namespace ui {

namespace {
    struct UdpClientGlobals {
        std::unique_ptr<asio::io_context> io;
        std::unique_ptr<asio::ip::udp::socket> sock;
        asio::ip::udp::endpoint server;
    } g;
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
    if (button({(float)x, (float)startY, (float)btnWidth, (float)btnHeight}, "Singleplayer", baseFont, BLACK, LIGHTGRAY, GRAY)) {
        screen = ScreenState::Singleplayer;
    }
    if (button({(float)x, (float)(startY + (btnHeight + gap) * 1), (float)btnWidth, (float)btnHeight}, "Multiplayer", baseFont, BLACK, LIGHTGRAY, GRAY)) {
        screen = ScreenState::Multiplayer;
        _focusedField = 0;
    }
    if (button({(float)x, (float)(startY + (btnHeight + gap) * 2), (float)btnWidth, (float)btnHeight}, "Options", baseFont, BLACK, LIGHTGRAY, GRAY)) {
        screen = ScreenState::Options;
    }
    if (button({(float)x, (float)(startY + (btnHeight + gap) * 3), (float)btnWidth, (float)btnHeight}, "Leaderboard", baseFont, BLACK, LIGHTGRAY, GRAY)) {
        screen = ScreenState::Leaderboard;
    }
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
                        auto* rh = reinterpret_cast<rtype::net::Header*>(in.data());
                        if (rh->version == rtype::net::ProtocolVersion && rh->type == rtype::net::MsgType::HelloAck) {
                            ok = true;
                            break;
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

// --- Minimal gameplay networking and rendering ---

void Screens::ensureNetSetup() {
    if (g.io) return;
    g.io = std::make_unique<asio::io_context>();
    g.sock = std::make_unique<asio::ip::udp::socket>(*g.io);
    g.sock->open(asio::ip::udp::v4());
    asio::ip::udp::resolver resolver(*g.io);
    g.server = *resolver.resolve(asio::ip::udp::v4(), _serverAddr, _serverPort).begin();
    g.sock->non_blocking(true);
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

void Screens::teardownNet() {
    if (g.sock && g.sock->is_open()) {
        asio::error_code ec; g.sock->close(ec);
    }
    g.sock.reset();
    g.io.reset();
}

void Screens::sendInput(std::uint8_t bits) {
    if (!g.sock) return;
    rtype::net::Header hdr{};
    hdr.version = rtype::net::ProtocolVersion;
    hdr.type = rtype::net::MsgType::Input;
    rtype::net::InputPacket ip{}; ip.sequence = 0; ip.bits = bits;
    hdr.size = sizeof(ip);
    std::array<char, sizeof(hdr) + sizeof(ip)> buf{};
    std::memcpy(buf.data(), &hdr, sizeof(hdr));
    std::memcpy(buf.data() + sizeof(hdr), &ip, sizeof(ip));
    g.sock->send_to(asio::buffer(buf), g.server);
}

void Screens::pumpNetworkOnce() {
    if (!g.sock) return;
    asio::ip::udp::endpoint from;
    std::array<char, 8192> in{};
    asio::error_code ec;
    std::size_t n = g.sock->receive_from(asio::buffer(in), from, 0, ec);
    if (ec == asio::error::would_block) return;
    if (ec || n < sizeof(rtype::net::Header)) return;
    auto* h = reinterpret_cast<const rtype::net::Header*>(in.data());
    if (h->version != rtype::net::ProtocolVersion) return;
    if (h->type == rtype::net::MsgType::State) {
        const char* p = in.data() + sizeof(rtype::net::Header);
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
        return;
    } else if (h->type == rtype::net::MsgType::Roster) {
        const char* p = in.data() + sizeof(rtype::net::Header);
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
        return;
    } else if (h->type == rtype::net::MsgType::LivesUpdate) {
        const char* p = in.data() + sizeof(rtype::net::Header);
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
        return;
    } else if (h->type == rtype::net::MsgType::ScoreUpdate) {
        const char* p = in.data() + sizeof(rtype::net::Header);
        if (n < sizeof(rtype::net::Header) + sizeof(rtype::net::ScoreUpdatePayload)) return;
        auto* su = reinterpret_cast<const rtype::net::ScoreUpdatePayload*>(p);
        if (su->id == _selfId) {
            _score = su->score;
        }
        return;
    } else {
        return;
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

    // Transition to gameplay when at least 2 players are present
    if (playerCount >= 2) {
        screen = ScreenState::Gameplay;
    }
}

void Screens::drawGameplay(ScreenState& screen) {
    if (!_connected) {
        titleCentered("Not connected. Press ESC.", GetScreenHeight()*0.5f, 24, RAYWHITE);
        return;
    }
    ensureNetSetup();

    // Input bits
    std::uint8_t bits = 0;
    if (IsKeyDown(KEY_LEFT))  bits |= rtype::net::InputLeft;
    if (IsKeyDown(KEY_RIGHT)) bits |= rtype::net::InputRight;
    if (IsKeyDown(KEY_UP))    bits |= rtype::net::InputUp;
    if (IsKeyDown(KEY_DOWN))  bits |= rtype::net::InputDown;
    if (IsKeyDown(KEY_SPACE)) bits |= rtype::net::InputShoot;

    double now = GetTime();
    if (now - _lastSend > 1.0/30.0) {
        sendInput(bits);
        _lastSend = now;
    }

    pumpNetworkOnce();

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

    // Height reserved by top area
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

    // Right: Level (Niveau)
    std::string lvlStr = "Niveau " + std::to_string(_level);
    int lvlW = MeasureText(lvlStr.c_str(), hudFont);
    int lvlX = w - margin - lvlW;
    DrawText(lvlStr.c_str(), lvlX, textY, hudFont, (Color){200, 200, 80, 255});

    // Playable area bounds (avoid overlapping top and bottom UI)
    int playableMinY = topReserved;
    int playableMaxY = h - bottomBarH;

    if (_entities.empty()) {
        titleCentered("Connecting to game...", (int)(GetScreenHeight()*0.5f), 24, RAYWHITE);
    }

    // Render entities as simple shapes
    for (auto& e : _entities) {
        Color c = {(unsigned char)((e.rgba>>24)&0xFF),(unsigned char)((e.rgba>>16)&0xFF),(unsigned char)((e.rgba>>8)&0xFF),(unsigned char)(e.rgba&0xFF)};
        if (e.type == 1) { // Player (on applique les contraintes)
            // Taille du vaisseau
            int shipW = 20, shipH = 12;
            // Empêcher de passer dans les barres HUD
            if (e.y < playableMinY) e.y = (float)playableMinY;
            if (e.y + shipH > playableMaxY) e.y = (float)(playableMaxY - shipH);
            // Empêcher de sortir de l'écran horizontalement
            if (e.x < 0) e.x = 0;
            if (e.x + shipW > w) e.x = (float)(w - shipW);
            DrawRectangle((int)e.x, (int)e.y, shipW, shipH, c);
        } else if (e.type == 2) { // Enemy
            // Enemies: clamp to playable vertical area so they don't overlap HUD or go below screen
            int ew = 18, eh = 12;
            if (e.y < playableMinY) e.y = (float)playableMinY;
            if (e.y + eh > playableMaxY) e.y = (float)(playableMaxY - eh);
            DrawRectangle((int)e.x, (int)e.y, ew, eh, c);
        } else if (e.type == 3) { // Bullet
            DrawRectangle((int)e.x, (int)e.y, 6, 3, c);
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

} // namespace ui
} // namespace client
