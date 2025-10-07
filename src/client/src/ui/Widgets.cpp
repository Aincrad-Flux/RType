#include "../../include/client/ui/Widgets.hpp"
#include <raylib.h>

namespace client {
namespace ui {

bool button(const Rectangle& bounds, const char* label, int fontSize, Color fg, Color bg, Color hoverBg) {
    Vector2 mouse = GetMousePosition();
    bool hovered = CheckCollisionPointRec(mouse, bounds);
    Color fill = hovered ? hoverBg : bg;
    DrawRectangleRec(bounds, fill);
    int textWidth = MeasureText(label, fontSize);
    int textX = (int)(bounds.x + (bounds.width - textWidth) / 2);
    int textY = (int)(bounds.y + (bounds.height - fontSize) / 2);
    DrawText(label, textX, textY, fontSize, fg);
    return hovered && IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
}

bool inputBox(const Rectangle& bounds, const char* label, std::string& text, bool focused, int fontSize, Color fg, Color bg, Color border, bool numericOnly) {
    DrawRectangleRec(bounds, bg);
    DrawRectangleLinesEx(bounds, 2.0f, focused ? RAYWHITE : border);
    DrawText(label, (int)bounds.x, (int)(bounds.y - fontSize - 6), fontSize, fg);
    std::string display = text;
    if (focused) {
        if (((int)(GetTime() * 2.0)) % 2 == 0) display.push_back('|');
    }
    int padding = 8;
    int textY = (int)(bounds.y + (bounds.height - fontSize) / 2);
    DrawText(display.c_str(), (int)bounds.x + padding, textY, fontSize, fg);

    if (focused) {
        int codepoint = GetCharPressed();
        while (codepoint > 0) {
            if (codepoint >= 32 && codepoint <= 126) {
                char c = (char)codepoint;
                bool ok = true;
                if (numericOnly) ok = (c >= '0' && c <= '9');
                if (ok) text.push_back(c);
            }
            codepoint = GetCharPressed();
        }
        if (IsKeyPressed(KEY_BACKSPACE) && !text.empty()) text.pop_back();
    }

    bool clicked = IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && CheckCollisionPointRec(GetMousePosition(), bounds);
    return clicked;
}

void titleCentered(const char* title, int y, int fontSize, Color color) {
    int w = MeasureText(title, fontSize);
    DrawText(title, (GetScreenWidth() - w) / 2, y, fontSize, color);
}

} // namespace ui
} // namespace client


