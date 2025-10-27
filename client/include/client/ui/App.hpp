#pragma once
#include "Screens.hpp"

namespace client {
namespace ui {

class App {
public:
    App();
    // Enable autoconnect to multiplayer on startup (optional)
    void setAutoConnect(const std::string& host, const std::string& port, const std::string& name);
    void run();
private:
    ScreenState _screen;
    MultiplayerForm _form;
    SingleplayerForm _singleForm;
    Screens _screens;
    bool _resizedForGameplay = false;
    bool _autoConnectPending = false;
};

} // namespace ui
} // namespace client
