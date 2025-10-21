#include "Screens.hpp"
#include "widgets/Title.hpp"
#include <raylib.h>
#include <algorithm>
#include "common/Protocol.hpp"

namespace client { namespace ui {

void Screens::drawGameplay(ScreenState& screen) {
    if (!assetsAvailable()) {
        int h = GetScreenHeight();
        int baseFont = (int)(h * 0.045f); if (baseFont < 16) baseFont = 16;
        titleCentered("Spritesheets not found.", (int)(h * 0.40f), (int)(h * 0.08f), RED);
        titleCentered("Place the sprites/ folder next to the executable then press ESC.", (int)(h * 0.52f), baseFont, RAYWHITE);
        if (IsKeyPressed(KEY_ESCAPE)) { leaveSession(); screen = ScreenState::Menu; }
        return;
    }
    if (!_connected) {
        titleCentered("Not connected. Press ESC.", GetScreenHeight()*0.5f, 24, RAYWHITE);
        return;
    }
    ensureNetSetup();
    pumpNetworkOnce();
    if (_serverReturnToMenu) { leaveSession(); screen = ScreenState::NotEnoughPlayers; return; }
    pumpNetworkOnce();
    if (_serverReturnToMenu) { leaveSession(); screen = ScreenState::NotEnoughPlayers; return; }
    if (!_sheetLoaded) loadSprites();
    if (!_enemyLoaded) loadEnemySprites();
    pumpNetworkOnce();

    int gw = GetScreenWidth();
    int gh = GetScreenHeight();
    int ghudFont = (int)(gh * 0.045f); if (ghudFont < 16) ghudFont = 16;
    int gmargin = 16;
    int gTopReserved = gmargin + ghudFont + gmargin;
    int gBottomBarH = std::max((int)(gh * 0.10f), ghudFont + gmargin);
    int gPlayableMinY = gTopReserved;
    int gPlayableMaxY = gh - gBottomBarH;
    if (gPlayableMaxY < gPlayableMinY + 1) gPlayableMaxY = gPlayableMinY + 1;
    const PackedEntity* self = nullptr;
    for (const auto& ent : _entities) { if (ent.type == 1 && ent.id == _selfId) { self = &ent; break; } }
    std::uint8_t bits = 0;
    bool wantLeft  = IsKeyDown(KEY_LEFT);
    bool wantRight = IsKeyDown(KEY_RIGHT);
    bool wantUp    = IsKeyDown(KEY_UP);
    bool wantDown  = IsKeyDown(KEY_DOWN);
    bool wantShoot = IsKeyDown(KEY_SPACE);
    if (self) {
        const float playerScale = 1.18f;
        float drawW = (_sheetLoaded && _frameW > 0) ? (_frameW * playerScale) : 24.0f;
        float drawH = (_sheetLoaded && _frameH > 0) ? (_frameH * playerScale) : 14.0f;
        const float xOffset = -6.0f;
        float dstX = self->x + xOffset;
        float dstY = self->y;
        float minX = 0.0f;
        float maxX = (float)gw - drawW;
        float minY = (float)gPlayableMinY;
        float maxY = (float)gPlayableMaxY - drawH;
        if (wantLeft  && dstX > minX) bits |= rtype::net::InputLeft;
        if (wantRight && dstX < maxX) bits |= rtype::net::InputRight;
        if (wantUp    && dstY > minY) bits |= rtype::net::InputUp;
        if (wantDown  && dstY < maxY) bits |= rtype::net::InputDown;
    } else {
        if (wantLeft)  bits |= rtype::net::InputLeft;
        if (wantRight) bits |= rtype::net::InputRight;
        if (wantUp)    bits |= rtype::net::InputUp;
        if (wantDown)  bits |= rtype::net::InputDown;
    }
    if (IsKeyPressed(KEY_LEFT_CONTROL) || IsKeyPressed(KEY_RIGHT_CONTROL)) {
        _shotMode = (_shotMode == ShotMode::Normal) ? ShotMode::Charge : ShotMode::Normal;
    }
    bool chargeHeld = (_shotMode == ShotMode::Charge) && IsKeyDown(KEY_SPACE);
    if (chargeHeld) {
        if (!_isCharging) { _isCharging = true; _chargeStart = GetTime(); }
    } else {
        if (_isCharging && IsKeyReleased(KEY_SPACE)) {
            double chargeDur = std::min(2.0, std::max(0.1, GetTime() - _chargeStart));
            _beamActive = true;
            _beamEndTime = GetTime() + 0.25;
            float px = self ? self->x : 0.0f;
            float py = self ? self->y : (float)((gPlayableMinY + gPlayableMaxY) / 2);
            _beamX = px; _beamY = py;
            _beamThickness = (float)(8.0 + chargeDur * 22.0);
        }
        _isCharging = false;
    }
    if (wantShoot && _shotMode == ShotMode::Normal) bits |= rtype::net::InputShoot;
    if (chargeHeld) bits |= rtype::net::InputCharge;

    double now = GetTime();
    if (now - _lastSend > 1.0/30.0) { sendInput(bits); _lastSend = now; }

    int w = GetScreenWidth();
    int h = GetScreenHeight();
    int hudFont = (int)(h * 0.045f); if (hudFont < 16) hudFont = 16; int margin = 16;
    int topY = margin; int xCursor = margin; int shown = 0;
    for (size_t i = 0; i < _otherPlayers.size() && shown < 3; ++i, ++shown) {
        const auto& op = _otherPlayers[i];
        DrawText(op.name.c_str(), xCursor, topY, hudFont, RAYWHITE);
        int nameW = MeasureText(op.name.c_str(), hudFont);
        xCursor += nameW + 12;
        int iconSize = std::max(6, hudFont / 2 - 2);
        int iconGap  = std::max(3, iconSize / 3);
        int livesToDraw = std::min(3, std::max(0, op.lives));
        int iconsW = livesToDraw * iconSize + (livesToDraw > 0 ? (livesToDraw - 1) * iconGap : 0);
        int iconY = topY + (hudFont - iconSize) / 2;
        for (int k = 0; k < livesToDraw; ++k) {
            int ix = xCursor + k * (iconSize + iconGap);
            DrawRectangle(ix, iconY, iconSize, iconSize, (Color){220, 80, 80, 255});
        }
        xCursor += iconsW + 24;
    }
    if (_otherPlayers.size() > 3) { std::string more = "x " + std::to_string(_otherPlayers.size() - 3); DrawText(more.c_str(), xCursor, topY, hudFont, LIGHTGRAY); }
    int topReserved = topY + hudFont + margin;
    int bottomBarH = std::max((int)(h * 0.10f), hudFont + margin);
    int bottomY = h - bottomBarH;
    DrawRectangle(0, bottomY, w, bottomBarH, (Color){20, 20, 20, 200});
    DrawRectangleLines(0, bottomY, w, bottomBarH, (Color){60, 60, 60, 255});
    int iconSize = std::max(10, std::min(18, bottomBarH - margin - 10));
    int iconGap  = std::max(4, iconSize / 3);
    int livesToDraw = std::min(10, std::max(0, _playerLives));
    int iconRowY = bottomY + (bottomBarH - iconSize) / 2;
    int iconRowX = margin;
    for (int i = 0; i < 10; ++i) {
        Color c = (i < livesToDraw) ? (Color){220, 80, 80, 255} : (Color){70, 70, 70, 255};
        DrawRectangle(iconRowX + i * (iconSize + iconGap), iconRowY, iconSize, iconSize, c);
    }
    std::string scoreStr = "Score: " + std::to_string(_score);
    int scoreW = MeasureText(scoreStr.c_str(), hudFont);
    int scoreX = (w - scoreW) / 2;
    int textY = bottomY + (bottomBarH - hudFont) / 2;
    DrawText(scoreStr.c_str(), scoreX, textY, hudFont, RAYWHITE);
    const char* modeStr = (_shotMode == ShotMode::Normal) ? "Shot: Normal" : "Shot: Charge";
    int modeW = MeasureText(modeStr, hudFont);
    int modeX = w - margin - modeW;
    DrawText(modeStr, modeX, textY, hudFont, (Color){200, 200, 80, 255});
    int playableMinY = topReserved;
    int playableMaxY = h - bottomBarH;
    if (playableMaxY < playableMinY + 1) playableMaxY = playableMinY + 1;

    if (_entities.empty()) { titleCentered("Connecting to game...", (int)(GetScreenHeight()*0.5f), 24, RAYWHITE); }

    static constexpr float kEnemyAabbW = 27.0f;
    static constexpr float kEnemyAabbH = 18.0f;
    for (std::size_t i = 0; i < _entities.size(); ++i) {
        auto& e = _entities[i];
        Color c = {(unsigned char)((e.rgba>>24)&0xFF),(unsigned char)((e.rgba>>16)&0xFF),(unsigned char)((e.rgba>>8)&0xFF),(unsigned char)(e.rgba&0xFF)};
        if (e.type == 1) {
            int shipW = 20, shipH = 12;
            if (e.y < playableMinY) e.y = (float)playableMinY;
            if (e.y + shipH > playableMaxY) e.y = (float)(playableMaxY - shipH);
            if (e.x < 0) e.x = 0;
            if (e.x + shipW > w) e.x = (float)(w - shipW);
            int rowIndex;
            auto it = _spriteRowById.find(e.id);
            if (it == _spriteRowById.end()) { rowIndex = _nextSpriteRow % _sheetRows; _spriteRowById[e.id] = rowIndex; _nextSpriteRow++; }
            else { rowIndex = it->second; }
            if (_sheetLoaded && _frameW > 0 && _frameH > 0) {
                int colIndex = 2; const float playerScale = 1.18f; float drawW = _frameW * playerScale; float drawH = _frameH * playerScale;
                const float xOffset = -6.0f; float dstX = e.x + xOffset; float dstY = e.y;
                if (dstY < playableMinY) dstY = (float)playableMinY;
                if (dstX < 0) dstX = 0; if (dstX + drawW > w) dstX = (float)(w - drawW);
                if (dstY + drawH > playableMaxY) dstY = (float)(playableMaxY - drawH);
                Rectangle src{ _frameW * colIndex, _frameH * rowIndex, _frameW, _frameH };
                Rectangle dst{ dstX, dstY, drawW, drawH };
                Vector2 origin{ 0.0f, 0.0f };
                DrawTexturePro(_sheet, src, dst, origin, 0.0f, WHITE);
            }
        } else if (e.type == 2) {
            if (_enemyLoaded && _enemyFrameW > 0 && _enemyFrameH > 0) {
                const float colIndex = 2.5f; const int rowIndex = 2; const float cropPx = 10.0f; float srcH = _enemyFrameH - cropPx; if (srcH < 1.0f) srcH = 1.0f;
                float ex = e.x; float ey = e.y; if (ey < playableMinY || ey + kEnemyAabbH > playableMaxY) continue; if (ex < 0 || ex + kEnemyAabbW > w) continue;
                Rectangle src{ _enemyFrameW * colIndex, _enemyFrameH * rowIndex, _enemyFrameW, srcH };
                Rectangle dst{ ex, ey, kEnemyAabbW, kEnemyAabbH };
                Vector2 origin{ 0.0f, 0.0f };
                DrawTexturePro(_enemySheet, src, dst, origin, 0.0f, WHITE);
            }
        } else if (e.type == 3) {
            DrawRectangle((int)e.x, (int)e.y, 6, 3, c);
        }
    }

    if (_beamActive) {
        if (GetTime() > _beamEndTime) { _beamActive = false; }
        else {
            float bx = _beamX; float by = _beamY; if (by < playableMinY) by = (float)playableMinY; if (by > playableMaxY) by = (float)playableMaxY;
            float halfT = _beamThickness * 0.5f; float y0 = std::max((float)playableMinY, by - halfT); float y1 = std::min((float)playableMaxY, by + halfT);
            if (y1 > y0) {
                DrawRectangle((int)bx, (int)y0, w - (int)bx, (int)(y1 - y0), (Color){120, 200, 255, 220});
                DrawRectangle((int)bx, (int)(y0 - 4), w - (int)bx, 4, (Color){120, 200, 255, 120});
                DrawRectangle((int)bx, (int)y1, w - (int)bx, 4, (Color){120, 200, 255, 120});
            }
        }
    }

    if (_gameOver) {
        DrawRectangle(0, 0, w, h, (Color){0, 0, 0, 180});
        titleCentered("GAME OVER", (int)(h * 0.40f), (int)(h * 0.10f), RED);
        std::string finalScore = "Score: " + std::to_string(_score);
        titleCentered(finalScore.c_str(), (int)(h * 0.52f), (int)(h * 0.06f), RAYWHITE);
        titleCentered("Press ESC to return to menu", (int)(h * 0.62f), (int)(h * 0.04f), LIGHTGRAY);
        if (IsKeyPressed(KEY_ESCAPE)) { teardownNet(); _connected = false; _entities.clear(); _gameOver = false; screen = ScreenState::Menu; return; }
    }
}

} } // namespace client::ui
