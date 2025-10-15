#include "../../../include/client/ui/widgets/Title.hpp"
#include <raylib.h>

namespace client { namespace ui {

void titleCentered(const char* title, int y, int fontSize, Color color) {
    int w = MeasureText(title, fontSize);
    DrawText(title, (GetScreenWidth() - w) / 2, y, fontSize, color);
}

} } // namespace client::ui
