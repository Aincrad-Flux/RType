// Backward-compat umbrella header; prefer including individual headers from widgets/
#pragma once
#include "widgets/Button.hpp"
#include "widgets/InputBox.hpp"
#include "widgets/Title.hpp"


bool button(const Rectangle& bounds, const char* label, int fontSize, Color fg, Color bg, Color hoverBg);

