#pragma once
#include "Screens.hpp"

namespace client {
namespace ui {

class App {
public:
    App();
    void run();
private:
    ScreenState _screen;
    MultiplayerForm _form;
    SingleplayerForm _singleForm;
    Screens _screens;
    bool _resizedForGameplay = false;
};

} // namespace ui
} // namespace client


