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

    // Connected players based on roster info (self + others)
    int playerCount = (int)_otherPlayers.size() + (_selfId ? 1 : 0);

    // Simplified lobby (no explicit host): server auto-starts when enough players join
    titleCentered("Waiting for players...", (int)(h * 0.22f), (int)(h * 0.08f), RAYWHITE);
    std::string sub = "Players connected: " + std::to_string(playerCount) + "/2";
    titleCentered(sub.c_str(), (int)(h * 0.34f), baseFont, RAYWHITE);

    int dots = ((int)(GetTime() * 2)) % 4;
    std::string hint = "The game will start automatically" + std::string(dots, '.');
    titleCentered(hint.c_str(), (int)(h * 0.46f), baseFont, LIGHTGRAY);

    int btnWidth = (int)(w * 0.18f);
    int btnHeight = (int)(h * 0.08f);
    int x = (w - btnWidth) / 2;
    int bottomMargin = std::max(10, (int)(h * 0.04f));
    int y = std::max(0, h - bottomMargin - btnHeight);
    Rectangle cancelBtn{(float)x, (float)y, (float)btnWidth, (float)btnHeight};
    if (button(cancelBtn, "Cancel", baseFont, BLACK, LIGHTGRAY, GRAY)) {
        teardownNet();
        _connected = false;
        _entities.clear();
        screen = ScreenState::Menu;
        return;
    }

    // Transition to gameplay when enough players are connected (server auto-start model)
    if (playerCount >= 2) {
        if (assetsAvailable()) { screen = ScreenState::Gameplay; }
        else { titleCentered("Missing spritesheet assets. Place sprites/ and try again.", (int)(h * 0.80f), baseFont, RED); }
    }
}

} } // namespace client::ui
