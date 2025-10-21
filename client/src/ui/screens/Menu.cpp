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

    Color disText = DARKGRAY;
    Color disBg = (Color){70, 70, 70, 255};
    Color disHover = disBg;

    Rectangle singleBtn{(float)x, (float)startY, (float)btnWidth, (float)btnHeight};
    if (button(singleBtn, "Singleplayer", baseFont, BLACK, LIGHTGRAY, GRAY)) {
        screen = ScreenState::Singleplayer;
        _focusedField = 0;
    }

    Rectangle multiBtn{(float)x, (float)(startY + (btnHeight + gap) * 1), (float)btnWidth, (float)btnHeight};
    if (button(multiBtn, "Multiplayer", baseFont, BLACK, LIGHTGRAY, GRAY)) {
        screen = ScreenState::Multiplayer;
        _focusedField = 0;
    }

    Rectangle optBtn{(float)x, (float)(startY + (btnHeight + gap) * 2), (float)btnWidth, (float)btnHeight};
    button(optBtn, "Options", baseFont, disText, disBg, disHover);
    Rectangle leadBtn{(float)x, (float)(startY + (btnHeight + gap) * 3), (float)btnWidth, (float)btnHeight};
    button(leadBtn, "Leaderboard", baseFont, disText, disBg, disHover);

    Rectangle quitBtn{(float)x, (float)(startY + (btnHeight + gap) * 4), (float)btnWidth, (float)btnHeight};
    if (button(quitBtn, "Quit", baseFont, BLACK, (Color){200, 80, 80, 255}, (Color){230, 120, 120, 255})) {
        screen = ScreenState::Exiting;
    }
}

} } // namespace client::ui
