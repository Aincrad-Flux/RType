#include "../../include/client/ui/Screens.hpp"
#include "../../include/client/ui/Widgets.hpp"
#include <raylib.h>
#include <asio.hpp>
#include <vector>
#include <cstring>
#include "common/Protocol.hpp"

namespace client {
namespace ui {

static int baseFontFromHeight(int h) {
    int baseFont = (int)(h * 0.045f);
    if (baseFont < 16) baseFont = 16;
    return baseFont;
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
            TraceLog(LOG_INFO, "Connecting to %s:%s as %s", form.serverAddress.c_str(), form.serverPort.c_str(), form.username.c_str());
            // Perform a simple UDP handshake synchronously
            try {
                asio::io_context io;
                asio::ip::udp::socket sock(io);
                sock.open(asio::ip::udp::v4());
                asio::ip::udp::resolver resolver(io);
                asio::ip::udp::endpoint endpoint = *resolver.resolve(asio::ip::udp::v4(), form.serverAddress, form.serverPort).begin();

                // Build Hello packet with username payload
                rtype::net::Header hdr{};
                hdr.version = rtype::net::ProtocolVersion;
                hdr.type = rtype::net::MsgType::Hello;
                hdr.size = static_cast<std::uint16_t>(form.username.size());
                std::vector<char> out(sizeof(rtype::net::Header) + form.username.size());
                std::memcpy(out.data(), &hdr, sizeof(hdr));
                std::memcpy(out.data() + sizeof(hdr), form.username.data(), form.username.size());

                sock.send_to(asio::buffer(out), endpoint);

                // Set a short timeout
                sock.non_blocking(true);
                asio::ip::udp::endpoint from;
                std::array<char, 1024> in{};
                double start = GetTime();
                bool ok = false;
                while (GetTime() - start < 1.0) {
                    asio::error_code ec;
                    std::size_t n = sock.receive_from(asio::buffer(in), from, 0, ec);
                    if (!ec && n >= sizeof(rtype::net::Header)) {
                        auto* rh = reinterpret_cast<rtype::net::Header*>(in.data());
                        if (rh->version == rtype::net::ProtocolVersion && rh->type == rtype::net::MsgType::HelloAck) {
                            ok = true;
                            break;
                        }
                    }
                }
                _statusMessage = ok ? std::string("Player Connected.") : std::string("Connection failed.");
            } catch (const std::exception& e) {
                _statusMessage = std::string("Error: ") + e.what();
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

} // namespace ui
} // namespace client


