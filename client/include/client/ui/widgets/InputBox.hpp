#pragma once
#include <raylib.h>
#include <string>

namespace client {
namespace ui {

// Draws a labeled input box; returns true if the box was clicked (to focus)
bool inputBox(const Rectangle& bounds, const char* label, std::string& text, bool focused, int fontSize, Color fg, Color bg, Color border, bool numericOnly);

} // namespace ui
} // namespace client
