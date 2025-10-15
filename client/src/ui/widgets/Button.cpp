#include "../../../include/client/ui/widgets/Button.hpp"
#include <raylib.h>

namespace client { namespace ui {

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

} } // namespace client::ui
