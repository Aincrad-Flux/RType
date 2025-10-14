#pragma once
#include <raylib.h>
#include <string>

namespace client {
namespace ui {

bool button(const Rectangle& bounds, const char* label, int fontSize, Color fg, Color bg, Color hoverBg);
bool inputBox(const Rectangle& bounds, const char* label, std::string& text, bool focused, int fontSize, Color fg, Color bg, Color border, bool numericOnly);
void titleCentered(const char* title, int y, int fontSize, Color color);

} // namespace ui
} // namespace client


