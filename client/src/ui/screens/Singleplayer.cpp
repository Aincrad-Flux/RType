#include "Screens.hpp"
#include "widgets/Button.hpp"
#include "widgets/InputBox.hpp"
#include "widgets/Title.hpp"
#include <raylib.h>

namespace client { namespace ui {

static int baseFontFromHeight(int h) {
    int baseFont = (int)(h * 0.045f);
    if (baseFont < 16) baseFont = 16;
    return baseFont;
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
    Rectangle startBtn{(float)btnX, (float)btnY, (float)btnWidth, (float)btnHeight};
    if (button(startBtn, "Start", baseFont, BLACK, startBg, startHover)) {
        if (canStart) {
            TraceLog(LOG_INFO, "Starting singleplayer as %s", form.username.c_str());
            screen = ScreenState::Menu;
        }
    }
    Rectangle backBtn{(float)(btnX + btnWidth + btnGap), (float)btnY, (float)btnWidth, (float)btnHeight};
    if (button(backBtn, "Back", baseFont, BLACK, LIGHTGRAY, GRAY)) {
        screen = ScreenState::Menu;
    }
}

} } // namespace client::ui
