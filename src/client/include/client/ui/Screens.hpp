#pragma once
#include <string>

namespace client {
namespace ui {

enum class ScreenState {
    Menu,
    Singleplayer,
    Multiplayer,
    Options,
    Leaderboard,
    Exiting
};

struct MultiplayerForm {
    std::string username;
    std::string serverAddress;
    std::string serverPort;
};

struct SingleplayerForm {
    std::string username;
};

class Screens {
public:
    void drawMenu(ScreenState& screen);
    void drawSingleplayer(ScreenState& screen, SingleplayerForm& form);
    void drawMultiplayer(ScreenState& screen, MultiplayerForm& form);
    void drawOptions();
    void drawLeaderboard();
private:
    int _focusedField = 0; // 0=user, 1=addr, 2=port
    std::string _statusMessage;
};

} // namespace ui
} // namespace client


