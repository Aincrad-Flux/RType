#include "Screens.hpp"
#include "widgets/Title.hpp"
#include <raylib.h>
#include <algorithm>
#include "common/Protocol.hpp"

namespace client { namespace ui {

void Screens::drawGameplay(ScreenState& screen) {
    // Multiplayer gameplay adapted to match singleplayer UI and feel
    if (!_connected) {
        titleCentered("Not connected. Press ESC.", GetScreenHeight()*0.5f, 24, RAYWHITE);
        if (IsKeyPressed(KEY_ESCAPE)) { leaveSession(); screen = ScreenState::Menu; }
        return;
    }
    ensureNetSetup();
    // Keep the latest snapshot fresh
    pumpNetworkOnce();
    if (_serverReturnToMenu) { leaveSession(); screen = ScreenState::NotEnoughPlayers; return; }

    // Compute playable band similar to singleplayer (reserve bottom bar height)
    int w = GetScreenWidth();
    int h = GetScreenHeight();
    int hudFont = std::max(16, (int)(h * 0.045f));
    int margin = 16;
    int bottomBarH = std::max((int)(h * 0.10f), hudFont + margin);
    int playableMinY = 0 + margin; // no top teammates bar in this layout
    int playableMaxY = h - bottomBarH;
    if (playableMaxY < playableMinY + 1) playableMaxY = playableMinY + 1;

    // Find self entity for input edge-gating
    const PackedEntity* self = nullptr;
    for (const auto& ent : _entities) { if (ent.type == 1 && ent.id == _selfId) { self = &ent; break; } }

    // Build input from arrows OR WASD, gate within playable area based on self pos
    bool wantLeft  = IsKeyDown(KEY_LEFT)  || IsKeyDown(KEY_A);
    bool wantRight = IsKeyDown(KEY_RIGHT) || IsKeyDown(KEY_D);
    bool wantUp    = IsKeyDown(KEY_UP)    || IsKeyDown(KEY_W);
    bool wantDown  = IsKeyDown(KEY_DOWN)  || IsKeyDown(KEY_S);
    bool wantShoot = IsKeyDown(KEY_SPACE);
    std::uint8_t bits = 0;
    // Assume player rect 24x16 like singleplayer for gating
    const float pW = 24.f, pH = 16.f;
    if (self) {
        float dstX = self->x;
        float dstY = self->y;
        if (wantLeft  && dstX > 0.0f) bits |= rtype::net::InputLeft;
        if (wantRight && dstX + pW < (float)w) bits |= rtype::net::InputRight;
        if (wantUp    && dstY > (float)playableMinY) bits |= rtype::net::InputUp;
        if (wantDown  && dstY + pH < (float)playableMaxY) bits |= rtype::net::InputDown;
    } else {
        if (wantLeft)  bits |= rtype::net::InputLeft;
        if (wantRight) bits |= rtype::net::InputRight;
        if (wantUp)    bits |= rtype::net::InputUp;
        if (wantDown)  bits |= rtype::net::InputDown;
    }

    // Simple overheat UI mechanic (client-side only): drains while firing, regens when not
    if (wantShoot) {
        _spHeat -= _spHeatDrainPerSec * GetFrameTime();
    } else {
        _spHeat += _spHeatRegenPerSec * GetFrameTime();
    }
    if (_spHeat < 0.f) _spHeat = 0.f; if (_spHeat > 1.f) _spHeat = 1.f;

    // Only send shoot input if not overheated (to mimic singleplayer feel)
    if (wantShoot && _spHeat > 0.f) bits |= rtype::net::InputShoot;

    // Send inputs at ~30Hz
    double now = GetTime();
    if (now - _lastSend > 1.0/30.0) { sendInput(bits); _lastSend = now; }

    // --- Bottom HUD: Lives (left) + Overheat bar (center) ---
    int bottomY = h - bottomBarH;
    DrawRectangle(0, bottomY, w, bottomBarH, (Color){0, 0, 0, 140});
    // Make HP squares smaller so they don't collide with the charge bar
    int sqSize = std::max(6, (int)((bottomBarH - 2 * margin) * 0.6f));
    int gap = std::max(6, sqSize / 3);
    int livesToDraw = std::min(10, std::max(0, _playerLives));
    int startX = margin;
    for (int i = 0; i < 10; ++i) {
        Color c = (i < livesToDraw) ? (Color){100, 220, 120, 255} : (Color){80, 80, 80, 180};
        DrawRectangle(startX + i * (sqSize + gap), bottomY + margin, sqSize, sqSize, c);
    }
    // Overheat bar centered
    int barW = (int)(w * 0.35f);
    int barX = (w - barW) / 2;
    int barY = bottomY + margin;
    int barH = sqSize;
    DrawRectangle(barX, barY, barW, barH, (Color){60, 60, 60, 180});
    int fillW = (int)(barW * _spHeat);
    Color fillC = _spHeat > 0.2f ? (Color){220, 90, 90, 220} : (Color){220, 40, 40, 240};
    DrawRectangle(barX, barY, fillW, barH, fillC);
    DrawRectangleLines(barX, barY, barW, barH, (Color){220, 220, 220, 200});

    // --- Team score (top-left) ---
    int hudFontScore = hudFont;
    int scoreMargin = margin;
    std::string scoreText = std::string("Score: ") + std::to_string(_score);
    DrawText(scoreText.c_str(), scoreMargin, scoreMargin, hudFontScore, RAYWHITE);

    // --- World rendering (rectangles like singleplayer) ---
    if (_entities.empty()) {
        titleCentered("Connecting to game...", (int)(GetScreenHeight()*0.5f), 24, RAYWHITE);
    }
    for (auto& e : _entities) {
        if (e.type == 1) {
            // Player ship
            float x = e.x, y = e.y;
            if (y < playableMinY) y = (float)playableMinY;
            if (y + pH > playableMaxY) y = (float)(playableMaxY - pH);
            if (x < 0) x = 0; if (x + pW > w) x = (float)(w - pW);
            DrawRectangle((int)x, (int)y, (int)pW, (int)pH, (Color){100, 200, 255, 255});
        } else if (e.type == 2) {
            // Enemy
            DrawRectangle((int)e.x, (int)e.y, 24, 16, (Color){220, 80, 80, 255});
        } else if (e.type == 3) {
            // Bullet
            DrawRectangle((int)e.x, (int)e.y, 6, 3, (Color){240, 220, 80, 255});
        }
    }

    // Game over overlay, back to menu on ESC
    if (_gameOver) {
        DrawRectangle(0, 0, w, h, (Color){0, 0, 0, 180});
        titleCentered("Game Over", (int)(h * 0.40f), (int)(h * 0.10f), RAYWHITE);
        if (IsKeyPressed(KEY_ESCAPE)) { teardownNet(); _connected = false; _entities.clear(); _gameOver = false; screen = ScreenState::Menu; return; }
    }
}

} } // namespace client::ui
