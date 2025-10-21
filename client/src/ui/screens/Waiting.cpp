#include "Screens.hpp"
#include "widgets/Button.hpp"
#include "widgets/Title.hpp"
#include <raylib.h>

namespace client { namespace ui {

static int baseFontFromHeight(int h) {
    int baseFont = (int)(h * 0.045f);
    if (baseFont < 16) baseFont = 16;
    return baseFont;
}

void Screens::drawWaiting(ScreenState& screen) {
    int w = GetScreenWidth();
    int h = GetScreenHeight();
    int baseFont = baseFontFromHeight(h);

    ensureNetSetup();
    pumpNetworkOnce();

    if (_serverReturnToMenu) {
        leaveSession();
        screen = ScreenState::NotEnoughPlayers;
        return;
    }

    int playerCount = 0;
    for (const auto& e : _entities) {
        if (e.type == 1) ++playerCount;
    }

    titleCentered("Waiting for players...", (int)(h * 0.25f), (int)(h * 0.08f), RAYWHITE);
    std::string sub = "Players connected: " + std::to_string(playerCount) + "/2";
    titleCentered(sub.c_str(), (int)(h * 0.40f), baseFont, RAYWHITE);

    int dots = ((int)(GetTime() * 2)) % 4;
    std::string hint = "The game will start automatically" + std::string(dots, '.');
    titleCentered(hint.c_str(), (int)(h * 0.50f), baseFont, LIGHTGRAY);

    int btnWidth = (int)(w * 0.18f);
    int btnHeight = (int)(h * 0.08f);
    int x = (w - btnWidth) / 2;
    int y = (int)(h * 0.70f);
    Rectangle cancelBtn{(float)x, (float)y, (float)btnWidth, (float)btnHeight};
    if (button(cancelBtn, "Cancel", baseFont, BLACK, LIGHTGRAY, GRAY)) {
        teardownNet();
        _connected = false;
        _entities.clear();
        screen = ScreenState::Menu;
        return;
    }

    if (playerCount >= 2) {
        if (assetsAvailable()) {
            screen = ScreenState::Gameplay;
        } else {
            titleCentered("Missing spritesheet assets. Place sprites/ and try again.", (int)(h * 0.80f), baseFont, RED);
        }
    }
}

} } // namespace client::ui
