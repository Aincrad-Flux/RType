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
    Rectangle backBtn{(float)x, (float)y, (float)btnWidth, (float)btnHeight};
    if (button(backBtn, "Back to Menu", baseFont, BLACK, LIGHTGRAY, GRAY)) {
        screen = ScreenState::Menu;
    }
}

} } // namespace client::ui
