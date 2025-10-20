#include "Screens.hpp"
#include "widgets/Button.hpp"
#include "widgets/InputBox.hpp"
#include "widgets/Title.hpp"
#include <raylib.h>

namespace client { namespace ui {

static int baseFontFromHeight(int h) {
    int baseFont = (int)(h * 0.045f);
    if (baseFont < 16) baseFont = 16;
    return baseFont;
}

void Screens::drawSingleplayer(ScreenState& screen, SingleplayerForm& /*form*/) {
    int w = GetScreenWidth();
    int h = GetScreenHeight();
    int baseFont = baseFontFromHeight(h);

    // Header
    titleCentered("Singleplayer", (int)(h * 0.10f), (int)(h * 0.08f), RAYWHITE);

    if (!_singleplayerActive) {
        // Idle screen with Start/Back
        int btnWidth = (int)(w * 0.22f);
        int btnHeight = (int)(h * 0.08f);
        int btnGap = (int)(w * 0.02f);
        int btnY = (int)(h * 0.45f);
        int btnX = (w - (btnWidth * 2 + btnGap)) / 2;

        Rectangle startBtn{(float)btnX, (float)btnY, (float)btnWidth, (float)btnHeight};
        if (button(startBtn, "Start", baseFont, BLACK, (Color){120, 200, 120, 255}, (Color){150, 230, 150, 255})) {
            initSingleplayerWorld();
            _singleplayerActive = true;
            _spPaused = false;
        }
        Rectangle backBtn{(float)(btnX + btnWidth + btnGap), (float)btnY, (float)btnWidth, (float)btnHeight};
        if (button(backBtn, "Back", baseFont, BLACK, LIGHTGRAY, GRAY)) {
            shutdownSingleplayerWorld();
            screen = ScreenState::Menu;
            return;
        }
        // Controls hint
        titleCentered("Controls: WASD/Arrows to move. ESC to pause.", (int)(h * 0.70f), baseFont, LIGHTGRAY);
    } else {
        // In-game: update and draw world if not paused
        if (IsKeyPressed(KEY_ESCAPE)) {
            _spPaused = !_spPaused;
        }

        if (!_spPaused) {
            float dt = GetFrameTime();
            updateSingleplayerWorld(dt);
        }
        drawSingleplayerWorld();

        // Pause overlay
        if (_spPaused) {
            DrawRectangle(0, 0, w, h, (Color){0, 0, 0, 160});
            titleCentered("Paused", (int)(h * 0.35f), (int)(h * 0.10f), RAYWHITE);
            int btnWidth = (int)(w * 0.22f);
            int btnHeight = (int)(h * 0.08f);
            int btnGap = (int)(w * 0.02f);
            int btnY = (int)(h * 0.55f);
            int btnX = (w - (btnWidth * 2 + btnGap)) / 2;
            if (button({(float)btnX, (float)btnY, (float)btnWidth, (float)btnHeight}, "Resume", baseFont, BLACK, LIGHTGRAY, GRAY)) {
                _spPaused = false;
            }
            if (button({(float)(btnX + btnWidth + btnGap), (float)btnY, (float)btnWidth, (float)btnHeight}, "Exit", baseFont, BLACK, (Color){200, 80, 80, 255}, (Color){230, 120, 120, 255})) {
                shutdownSingleplayerWorld();
                screen = ScreenState::Menu;
                return;
            }
        }
    }
}

// --- Local Singleplayer sandbox (Engine test) ---
void Screens::initSingleplayerWorld() {
    if (_spInitialized) return;
    _spWorld = std::make_unique<rt::ecs::Registry>();
    // Systems
    _spWorld->addSystem(std::make_unique<rt::systems::PlayerControlSystem>());
    _spWorld->addSystem(std::make_unique<rt::systems::MovementSystem>());
    // Player entity
    auto player = _spWorld->create();
    player.add<rt::components::Position>(100.f, 100.f);
    player.add<rt::components::Controller>();
    _spPlayer = player;
    _spInitialized = true;
}

void Screens::shutdownSingleplayerWorld() {
    _spWorld.reset();
    _spPlayer = 0;
    _spInitialized = false;
    _singleplayerActive = false;
    _spPaused = false;
}

void Screens::updateSingleplayerWorld(float dt) {
    if (!_spWorld) return;
    // Map keyboard to Controller bits
    constexpr std::uint8_t kUp = 1 << 0;
    constexpr std::uint8_t kDown = 1 << 1;
    constexpr std::uint8_t kLeft = 1 << 2;
    constexpr std::uint8_t kRight = 1 << 3;
    std::uint8_t bits = 0;
    if (IsKeyDown(KEY_W) || IsKeyDown(KEY_UP)) bits |= kUp;
    if (IsKeyDown(KEY_S) || IsKeyDown(KEY_DOWN)) bits |= kDown;
    if (IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT)) bits |= kLeft;
    if (IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT)) bits |= kRight;
    if (auto* c = _spWorld->get<rt::components::Controller>(_spPlayer)) c->bits = bits;
    _spWorld->update(dt);
}

void Screens::drawSingleplayerWorld() {
    if (!_spWorld) return;
    // draw player as a simple rectangle
    auto* p = _spWorld->get<rt::components::Position>(_spPlayer);
    if (p) {
        Rectangle rect{p->x, p->y, 24.f, 16.f};
        DrawRectangleRec(rect, (Color){100, 200, 255, 255});
    }
}

} } // namespace client::ui
