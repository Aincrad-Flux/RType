#include "Screens.hpp"
#include <iostream>

namespace client { namespace ui {

void Screens::logMessage(const std::string& msg, const char* level) {
    if (level)
        std::cout << "[" << level << "] " << msg << std::endl;
    else
        std::cout << "[INFO] " << msg << std::endl;
}

} } // namespace client::ui
