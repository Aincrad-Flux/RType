#include "../../include/client/ui/Screens.hpp"
#include "../../include/client/ui/Widgets.hpp"
#include <raylib.h>
#include <asio.hpp>
#include <memory>
#include <iostream>
#include <vector>
#include <algorithm>
#include <cstring>
#include <string>
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

Screens::~Screens() {
    if (_sheetLoaded) {
        UnloadTexture(_sheet);
        _sheetLoaded = false;
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
    // Reset sprite assignments when disconnecting
    _spriteRowById.clear();
    _nextSpriteRow = 0;
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
    if (ec || n < sizeof(rtype::net::Header)) return;
    auto* h = reinterpret_cast<const rtype::net::Header*>(in.data());
    if (h->version != rtype::net::ProtocolVersion) return;
    if (h->type != rtype::net::MsgType::State) return;
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

    // Lazy-load spritesheet
    if (!_sheetLoaded) loadSprites();

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

    // --- HUD ---
    int w = GetScreenWidth();
    int h = GetScreenHeight();
    int hudFont = (int)(h * 0.045f);
    if (hudFont < 16) hudFont = 16;
    int margin = 16;

    // Nom du joueur (en haut à gauche)
    DrawText(_username.c_str(), margin, margin, hudFont, RAYWHITE);

    // Niveau (Lv) à droite du nom
    int nameWidth = MeasureText(_username.c_str(), hudFont);
    int lv = 1; // TODO: remplacer par la vraie valeur du niveau si disponible
    std::string lvStr = "Lv " + std::to_string(lv);
    int lvX = margin + nameWidth + 16;
    DrawText(lvStr.c_str(), lvX, margin, hudFont, (Color){200, 200, 80, 255});

    // Barre de HP sur la même ligne, à droite du lvl
    float hp = 1.0f; // TODO: remplacer par la vraie valeur de HP (0.0 à 1.0)
    int lvWidth = MeasureText(lvStr.c_str(), hudFont);
    int barX = lvX + lvWidth + 32;
    int barY = margin + hudFont/4;
    int barH = hudFont/2;
    int barW = w - barX - margin;
    if (barW > 0) {
        DrawRectangle(barX, barY, barW, barH, DARKGRAY);
        DrawRectangle(barX, barY, (int)(barW * hp), barH, (Color){120, 220, 120, 255});
        DrawRectangleLines(barX, barY, barW, barH, RAYWHITE);
    }

    // Calculer la zone HUD pour empêcher le joueur de passer dessous
    int hudBottom = margin + hudFont;

    if (_entities.empty()) {
        titleCentered("Connecting to game...", (int)(GetScreenHeight()*0.5f), 24, RAYWHITE);
    }

    // Render entities using persistent sprite-row assignment per player id
    for (std::size_t i = 0; i < _entities.size(); ++i) {
        auto& e = _entities[i];
        Color c = {(unsigned char)((e.rgba>>24)&0xFF),(unsigned char)((e.rgba>>16)&0xFF),(unsigned char)((e.rgba>>8)&0xFF),(unsigned char)(e.rgba&0xFF)};
        if (e.type == 1) {
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
                if (e.y < hudBottom) e.y = (float)hudBottom;
                if (e.x < 0) e.x = 0;
                if (e.x + drawW > w) e.x = (float)(w - drawW);
                if (e.y + drawH > h) e.y = (float)(h - drawH);
                Rectangle src{ _frameW * colIndex, _frameH * rowIndex, _frameW, _frameH };
                Rectangle dst{ e.x, e.y, drawW, drawH };
                Vector2 origin{ 0.0f, 0.0f };
                DrawTexturePro(_sheet, src, dst, origin, 0.0f, WHITE);
            } else {
                int shipW = 24, shipH = 14;
                if (e.y < hudBottom) e.y = (float)hudBottom;
                if (e.x < 0) e.x = 0;
                if (e.x + shipW > w) e.x = (float)(w - shipW);
                if (e.y + shipH > h) e.y = (float)(h - shipH);
                DrawRectangle((int)e.x, (int)e.y, shipW, shipH, c);
            }
        } else if (e.type == 2) {
            int ew = 18, eh = 12;
            if (e.y < hudBottom) e.y = (float)hudBottom;
            if (e.y + eh > h) e.y = (float)(h - eh);
            DrawRectangle((int)e.x, (int)e.y, ew, eh, c);
        } else if (e.type == 3) {
            DrawRectangle((int)e.x, (int)e.y, 6, 3, c);
        }
    }
}

} // namespace ui
} // namespace client
