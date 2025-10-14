#include "../../include/client/ui/App.hpp"
#include <raylib.h>
#include <cmath>
using namespace client::ui;

static void drawStarfield(float t) {
    for (int i = 0; i < 300; ++i) {
        float x = fmodf((i * 73 + t * 60), (float)GetScreenWidth());
        float y = (i * 37) % GetScreenHeight();
        DrawPixel((int)x, (int)y, (i % 7 == 0) ? RAYWHITE : DARKGRAY);
    }
}

App::App() : _screen(ScreenState::Menu) {}

void App::run() {
    const int screenWidth = 960;
    const int screenHeight = 540;
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(screenWidth, screenHeight, "R-Type Client");
    // Prevent ESC from closing the whole window; we handle ESC ourselves
    SetExitKey(KEY_NULL);
    SetTargetFPS(60);

    float t = 0.f;
    while (!WindowShouldClose() && _screen != ScreenState::Exiting) {
        float dt = GetFrameTime();
        t += dt;

        if (IsKeyPressed(KEY_ESCAPE)) {
            if (_screen == ScreenState::Menu) {
                _screen = ScreenState::Exiting;
            } else {
                // Leaving current screen back to menu; ensure we leave any active session
                _screens.leaveSession();
                _screen = ScreenState::Menu;
            }
        }

        BeginDrawing();
        ClearBackground(BLACK);
        drawStarfield(t);

        switch (_screen) {
            case ScreenState::Menu: _screens.drawMenu(_screen); break;
            case ScreenState::Singleplayer: _screens.drawSingleplayer(_screen, _singleForm); break;
            case ScreenState::Multiplayer: _screens.drawMultiplayer(_screen, _form); break;
            case ScreenState::Waiting: _screens.drawWaiting(_screen); break;
            case ScreenState::Gameplay:
                if (!_resizedForGameplay) {
                    // Slightly increase height to make room for the bottom bar
                    SetWindowSize(screenWidth, (int)(screenHeight * 1.10f));
                    _resizedForGameplay = true;
                }
                _screens.drawGameplay(_screen);
                break;
            case ScreenState::Options: _screens.drawOptions(); break;
            case ScreenState::Leaderboard: _screens.drawLeaderboard(); break;
            case ScreenState::NotEnoughPlayers: _screens.drawNotEnoughPlayers(_screen); break;
            case ScreenState::Exiting: break;
        }

        EndDrawing();
    }

    // On exit, ensure we disconnect cleanly if needed
    _screens.leaveSession();
    // Release GPU resources before closing window
    _screens.unloadGraphics();

    CloseWindow();
}
