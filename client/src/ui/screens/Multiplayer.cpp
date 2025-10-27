#include "Screens.hpp"
#include "widgets/Button.hpp"
#include "widgets/InputBox.hpp"
#include "widgets/Title.hpp"
#include <raylib.h>
#include <asio.hpp>
#include <array>
#include <thread>
#include <chrono>
#include <cstring>
#include <string>
#include "common/Protocol.hpp"

namespace client { namespace ui {

static int baseFontFromHeight(int h) {
    int baseFont = (int)(h * 0.045f);
    if (baseFont < 16) baseFont = 16;
    return baseFont;
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
    int bottomMargin = std::max(10, (int)(h * 0.06f));
    int btnY = std::max(0, h - bottomMargin - btnHeight);
    int btnGap = (int)(w * 0.02f);
    int btnX = (w - (btnWidth * 2 + btnGap)) / 2;
    bool canConnect = !form.username.empty() && !form.serverAddress.empty() && !form.serverPort.empty();
    Color connectBg = canConnect ? (Color){120, 200, 120, 255} : (Color){80, 120, 80, 255};
    Color connectHover = canConnect ? (Color){150, 230, 150, 255} : (Color){90, 140, 90, 255};
    Rectangle connectBtn{(float)btnX, (float)btnY, (float)btnWidth, (float)btnHeight};
    if (button(connectBtn, "Connect", baseFont, BLACK, connectBg, connectHover)) {
        if (canConnect) {
            logMessage("Connecting to " + form.serverAddress + ":" + form.serverPort + " as " + form.username, "INFO");
            try {
                _username = form.username;
                _serverAddr = form.serverAddress;
                _serverPort = form.serverPort;
                _selfId = 0;
                _playerLives = 4;
                _gameOver = false;
                _otherPlayers.clear();

                teardownNet();
                ensureNetSetup();

                bool ok = waitHelloAck(1.0);
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
    Rectangle backBtn{(float)(btnX + btnWidth + btnGap), (float)btnY, (float)btnWidth, (float)btnHeight};
    if (button(backBtn, "Back", baseFont, BLACK, LIGHTGRAY, GRAY)) {
        screen = ScreenState::Menu;
    }
    if (!_statusMessage.empty()) {
        // Place status above the bottom buttons to avoid overlap
        titleCentered(_statusMessage.c_str(), std::max(0, btnY - (int)(h * 0.06f)), baseFont, RAYWHITE);
    }
}

} } // namespace client::ui
