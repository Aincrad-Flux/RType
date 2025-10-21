#pragma once
#include <raylib.h>

namespace client {
namespace ui {

// Draws a button and returns true on click (mouse down inside bounds)
bool button(const Rectangle& bounds, const char* label, int fontSize, Color fg, Color bg, Color hoverBg);

} // namespace ui
} // namespace client
