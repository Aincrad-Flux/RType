#include "Screens.hpp"
#include "widgets/Button.hpp"
#include "widgets/InputBox.hpp"
#include "widgets/Title.hpp"
#include <raylib.h>
#include <cmath>
#include <random>
#include <algorithm>

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

    // Header: show only when not actively playing
    if (!_singleplayerActive) {
        titleCentered("Singleplayer", (int)(h * 0.10f), (int)(h * 0.08f), RAYWHITE);
    }

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
        if (!_gameOver && IsKeyPressed(KEY_ESCAPE)) {
            _spPaused = !_spPaused;
        }

        if (!_spPaused) {
            float dt = GetFrameTime();
            updateSingleplayerWorld(dt);
        }
        drawSingleplayerWorld();

        // Overlays
        if (_gameOver) {
            DrawRectangle(0, 0, w, h, (Color){0, 0, 0, 180});
            titleCentered("Game Over", (int)(h * 0.35f), (int)(h * 0.10f), RAYWHITE);
            int btnWidth = (int)(w * 0.22f);
            int btnHeight = (int)(h * 0.08f);
            int btnGap = (int)(w * 0.02f);
            int btnY = (int)(h * 0.55f);
            int btnX = (w - (btnWidth * 2 + btnGap)) / 2;
            if (button({(float)btnX, (float)btnY, (float)btnWidth, (float)btnHeight}, "Restart", baseFont, BLACK, (Color){180, 180, 220, 255}, (Color){210, 210, 240, 255})) {
                shutdownSingleplayerWorld();
                initSingleplayerWorld();
                _singleplayerActive = true;
                return;
            }
            if (button({(float)(btnX + btnWidth + btnGap), (float)btnY, (float)btnWidth, (float)btnHeight}, "Exit", baseFont, BLACK, (Color){200, 80, 80, 255}, (Color){230, 120, 120, 255})) {
                shutdownSingleplayerWorld();
                screen = ScreenState::Menu;
                return;
            }
        } else if (_spPaused) {
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
    _spWorld->addSystem(std::make_unique<rt::systems::AiControlSystem>());
    _spWorld->addSystem(std::make_unique<rt::systems::MovementSystem>());
    _spWorld->addSystem(std::make_unique<rt::systems::CollisionSystem>());
    // Player entity
    auto player = _spWorld->create();
    player.add<rt::components::Position>(100.f, 100.f);
    player.add<rt::components::Controller>();
    player.add<rt::components::Player>();
    player.add<rt::components::Size>(24.f, 16.f);
    _spPlayer = player;

    // Reset score at the start of a singleplayer run
    _score = 0;

    // Reset power-ups and schedule first threshold between 1500 and 2000 points
    _spPowerups.clear();
    {
        std::uniform_int_distribution<int> dd(_spPowerupMinPts, _spPowerupMaxPts);
        _spNextPowerupScore = dd(_spRng);
    }

    // Start with an initial simple line
    _spEnemies.clear();
    _spBullets.clear();
    _spShootCooldown = 0.f;
    _spElapsed = 0.f;
    _spSpawnTimer = 0.f;
    _spNextFormation = 0;
    // Seed RNG with time
    std::random_device rd; _spRng.seed(rd());
    // Reset shield state
    _spInvincibleTimer = 0.f;
    // Schedule first spawn with random delay
    spScheduleNextSpawn();
    _spInitialized = true;
}

void Screens::shutdownSingleplayerWorld() {
    _spWorld.reset();
    _spPlayer = 0;
    _spEnemies.clear();
    _spBullets.clear();
    _spInitialized = false;
    _singleplayerActive = false;
    _spPaused = false;
    _gameOver = false;
    _spInvincibleTimer = 0.f;
}

void Screens::updateSingleplayerWorld(float dt) {
    if (!_spWorld) return;
    // Map keyboard to Controller bits (disabled on game over)
    constexpr std::uint8_t kUp = 1 << 0;
    constexpr std::uint8_t kDown = 1 << 1;
    constexpr std::uint8_t kLeft = 1 << 2;
    constexpr std::uint8_t kRight = 1 << 3;
    std::uint8_t bits = 0;
    if (!_gameOver) {
        if (IsKeyDown(KEY_W) || IsKeyDown(KEY_UP)) bits |= kUp;
        if (IsKeyDown(KEY_S) || IsKeyDown(KEY_DOWN)) bits |= kDown;
        if (IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT)) bits |= kLeft;
        if (IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT)) bits |= kRight;
    }
    if (auto* c = _spWorld->get<rt::components::Controller>(_spPlayer)) c->bits = bits;

    // Global timers
    _spElapsed += dt;
    _spSpawnTimer += dt;
    _spShootCooldown -= dt;
    if (_spShootCooldown < 0.f) _spShootCooldown = 0.f;
    // Tick invincibility frames
    if (_spHitIframes > 0.f) { _spHitIframes -= dt; if (_spHitIframes < 0.f) _spHitIframes = 0.f; }
    // Tick shield (invincibility power-up)
    if (_spInvincibleTimer > 0.f) { _spInvincibleTimer -= dt; if (_spInvincibleTimer < 0.f) _spInvincibleTimer = 0.f; }

    // Overheat: if firing, drain heat; otherwise regenerate
    bool shootHeld = !_gameOver && IsKeyDown(KEY_SPACE);
    if (shootHeld) {
        _spHeat -= _spHeatDrainPerSec * dt;
    } else {
        _spHeat += _spHeatRegenPerSec * dt;
    }
    if (_spHeat < 0.f) _spHeat = 0.f; if (_spHeat > 1.f) _spHeat = 1.f;

    // Player shooting: Space creates a bullet with cooldown; disabled when overheated (_spHeat == 0)
    if (shootHeld && _spShootCooldown <= 0.f && _spHeat > 0.f) {
        if (auto* pp = _spWorld->get<rt::components::Position>(_spPlayer)) {
            float bx = pp->x + 24.f; // from player front
            float by = pp->y + 6.f;  // roughly center
            _spBullets.push_back({_spBulletW > 0 ? bx : bx, by, _spBulletSpeed, 0.f, _spBulletW, _spBulletH});
            _spShootCooldown = _spShootInterval;
        }
    }

    // Randomized spawn scheduler, with cap on active enemies
    if (!_gameOver && _spSpawnTimer >= _spNextSpawnDelay && _spEnemies.size() < _spEnemyCap) {
        _spSpawnTimer = 0.f;
        // choose formation cyclically to keep variety while still randomizing timing
        int k = _spNextFormation++ % 4;
        int h = GetScreenHeight();
        float topMargin = h * 0.10f;
        float bottomMargin = h * 0.05f;
        float minY = topMargin + 40.f;
        float maxY = h - bottomMargin - 80.f;
        if (maxY < minY) maxY = minY + 1.f;
        std::uniform_real_distribution<float> ydist(minY, maxY);
        float y = ydist(_spRng);
        switch (k) {
            case 0: spSpawnLine(6, y); break;
            case 1: spSpawnSnake(6, y, 70.f, 2.2f, 36.f); break;
            case 2: spSpawnTriangle(5, y, 36.f); break;
            case 3: spSpawnDiamond(4, y, 36.f); break;
        }
        spScheduleNextSpawn();
    }

    // Drive AI bits for each enemy (prefer up-left overall)
    for (auto it = _spEnemies.begin(); it != _spEnemies.end(); ) {
        auto& en = *it;
        // If entity no longer exists (basic check: no Position), erase
        auto* pos = _spWorld->get<rt::components::Position>(en.id);
        auto* ai = _spWorld->get<rt::components::AiController>(en.id);
        if (!pos || !ai) { it = _spEnemies.erase(it); continue; }
        float t = _spElapsed - en.spawnTime;
        std::uint8_t bitsAI = 0;
        // On game over, freeze enemies
        if (!_gameOver) {
            // Always move to the left
            bitsAI |= kLeft;
        }
        switch (en.kind) {
            case SpFormationKind::Snake: {
                float phase = t * en.frequency + en.index * 0.5f;
                float s = std::sin(phase);
                if (!_gameOver) bitsAI |= (s > 0.f ? kUp : 0);
                // small downward occasionally to prevent sticking at top if extremely high
                if (!_gameOver && s <= 0.f && ((en.index + (int)(t)) % 3 == 0)) bitsAI |= kDown;
                break;
            }
            case SpFormationKind::Triangle:
            case SpFormationKind::Diamond:
            case SpFormationKind::Line: {
                // move straight left only for these formations
                break;
            }
        }
        ai->bits = bitsAI;
        // Despawn when far left off-screen
        if (pos->x < -80.f) { _spWorld->destroy(en.id); it = _spEnemies.erase(it); }
        else { ++it; }
    }

    // Update bullets and handle collisions
    const int screenW = GetScreenWidth();
    if (!_gameOver)
    for (std::size_t i = 0; i < _spBullets.size(); ) {
        auto& b = _spBullets[i];
        b.x += b.vx * dt;
        b.y += b.vy * dt;
        bool destroyBullet = false;
        // Offscreen to the right
        if (b.x > screenW + 50.f) destroyBullet = true;
        // Collide with enemies (AABB)
        for (std::size_t ei = 0; ei < _spEnemies.size() && !destroyBullet; ++ei) {
            auto& en = _spEnemies[ei];
            if (auto* ep = _spWorld->get<rt::components::Position>(en.id)) {
                float ex = ep->x, ey = ep->y, ew = 24.f, eh = 16.f;
                float bx1 = b.x, by1 = b.y, bx2 = b.x + b.w, by2 = b.y + b.h;
                float ex2 = ex + ew, ey2 = ey + eh;
                bool hit = !(bx2 < ex || ex2 < bx1 || by2 < ey || ey2 < by1);
                if (hit) {
                    // Enemy dies on hit: remove entity and from list
                    _spWorld->destroy(en.id);
                    _spEnemies.erase(_spEnemies.begin() + (long)ei);
                    // Award score: +50 per enemy killed
                    _score += 50;
                    // Check and spawn power-ups when crossing threshold(s)
                    while (_score >= _spNextPowerupScore) {
                        // Spawn a power-up from the right moving left
                        int h = GetScreenHeight();
                        float topMargin = h * 0.10f;
                        float bottomMargin = h * 0.05f;
                        float minY = topMargin + 16.f;
                        float maxY = h - bottomMargin - 16.f;
                        if (maxY < minY) maxY = minY + 1.f;
                        std::uniform_real_distribution<float> ydist(minY, maxY);
                        float y = ydist(_spRng);
                        float x = (float)screenW + _spPowerupRadius + 8.f;
                        // Choose type with 25% each among 4 slots (2 reserved for future types)
                        std::uniform_int_distribution<int> tdist(0, 3);
                        int slot = tdist(_spRng);
                        if (slot == 0) {
                            _spPowerups.push_back({x, y, -_spPowerupSpeed, _spPowerupRadius, SpPowerupType::Life});
                        } else if (slot == 1) {
                            _spPowerups.push_back({x, y, -_spPowerupSpeed, _spPowerupRadius, SpPowerupType::Invincibility});
                        } else if (slot == 2) {
                            _spPowerups.push_back({x, y, -_spPowerupSpeed, _spPowerupRadius, SpPowerupType::ClearBoard});
                        } else {
                            // slots 2 and 3 reserved for future power-ups: no spawn for now
                        }
                        // Schedule next threshold
                        std::uniform_int_distribution<int> dd(_spPowerupMinPts, _spPowerupMaxPts);
                        _spNextPowerupScore += dd(_spRng);
                    }
                    destroyBullet = true;
                }
            }
        }
        if (destroyBullet) _spBullets.erase(_spBullets.begin() + (long)i);
        else ++i;
    }

    // Move power-ups and handle pickup/offscreen
    if (!_gameOver) {
        // Player rectangle for collision
        float px = 0.f, py = 0.f, pw = 24.f, ph = 16.f;
        if (auto* pp = _spWorld->get<rt::components::Position>(_spPlayer)) { px = pp->x; py = pp->y; }
        auto rectCircleHit = [&](float cx, float cy, float r) {
            // Clamp circle center to rectangle bounds then compare distance
            float rx1 = px, ry1 = py, rx2 = px + pw, ry2 = py + ph;
            float closestX = std::clamp(cx, rx1, rx2);
            float closestY = std::clamp(cy, ry1, ry2);
            float dx = cx - closestX;
            float dy = cy - closestY;
            return (dx*dx + dy*dy) <= (r * r);
        };
        for (std::size_t i = 0; i < _spPowerups.size(); ) {
            auto& pu = _spPowerups[i];
            pu.x += pu.vx * dt;
            bool remove = false;
            // Pickup
            if (rectCircleHit(pu.x, pu.y, pu.radius)) {
                if (pu.type == SpPowerupType::Life) {
                    if (_playerLives < _maxLives) _playerLives += 1;
                } else if (pu.type == SpPowerupType::Invincibility) {
                    _spInvincibleTimer = _spInvincibleDuration;
                } else if (pu.type == SpPowerupType::ClearBoard) {
                    // Kill all enemies currently visible on screen and add 50 points per enemy
                    int screenW = GetScreenWidth();
                    int screenH = GetScreenHeight();
                    int killed = 0;
                    for (std::size_t ei = 0; ei < _spEnemies.size(); ) {
                        auto& en = _spEnemies[ei];
                        if (auto* ep = _spWorld->get<rt::components::Position>(en.id)) {
                            float ex = ep->x, ey = ep->y, ew = 24.f, eh = 16.f;
                            bool onScreen = !(ex + ew < 0.f || ex > (float)screenW || ey + eh < 0.f || ey > (float)screenH);
                            if (onScreen) {
                                _spWorld->destroy(en.id);
                                _spEnemies.erase(_spEnemies.begin() + (long)ei);
                                ++killed;
                                continue;
                            }
                        }
                        ++ei;
                    }
                    if (killed > 0) {
                        _score += 50 * killed;
                        // Check and spawn power-ups when crossing threshold(s)
                        while (_score >= _spNextPowerupScore) {
                            int h = GetScreenHeight();
                            float topMargin = h * 0.10f;
                            float bottomMargin = h * 0.05f;
                            float minY = topMargin + 16.f;
                            float maxY = h - bottomMargin - 16.f;
                            if (maxY < minY) maxY = minY + 1.f;
                            std::uniform_real_distribution<float> ydist(minY, maxY);
                            float y = ydist(_spRng);
                            float x = (float)screenW + _spPowerupRadius + 8.f;
                            std::uniform_int_distribution<int> tdist(0, 3);
                            int slot = tdist(_spRng);
                            if (slot == 0) {
                                _spPowerups.push_back({x, y, -_spPowerupSpeed, _spPowerupRadius, SpPowerupType::Life});
                            } else if (slot == 1) {
                                _spPowerups.push_back({x, y, -_spPowerupSpeed, _spPowerupRadius, SpPowerupType::Invincibility});
                            } else if (slot == 2) {
                                _spPowerups.push_back({x, y, -_spPowerupSpeed, _spPowerupRadius, SpPowerupType::ClearBoard});
                            } else {
                                // slot 3 reserved: no spawn
                            }
                            std::uniform_int_distribution<int> dd(_spPowerupMinPts, _spPowerupMaxPts);
                            _spNextPowerupScore += dd(_spRng);
                        }
                    }
                }
                remove = true;
            }
            // Offscreen
            if (pu.x + pu.radius < -20.f) remove = true;
            if (remove) _spPowerups.erase(_spPowerups.begin() + (long)i);
            else ++i;
        }
    }

    _spWorld->update(dt);

    // If engine marked player as collided, decrement life and clear flag
    if (auto* col = _spWorld->get<rt::components::Collided>(_spPlayer)) {
        if (col->value && _spHitIframes <= 0.f && _playerLives > 0 && _spInvincibleTimer <= 0.f) {
            _playerLives = std::max(0, _playerLives - 1);
            _spHitIframes = _spHitIframesDuration;
        }
        // Clear marker for next frame detection
        col->value = false;
    }
    if (_playerLives <= 0) {
        _gameOver = true;
    }
}

void Screens::drawSingleplayerWorld() {
    if (!_spWorld) return;
    // draw player as a simple rectangle
    auto* p = _spWorld->get<rt::components::Position>(_spPlayer);
    if (p) {
        Rectangle rect{p->x, p->y, 24.f, 16.f};
        DrawRectangleRec(rect, (Color){100, 200, 255, 255});
        // Draw shield if invincible
        if (_spInvincibleTimer > 0.f) {
            int cx = (int)(p->x + 12.f);
            int cy = (int)(p->y + 8.f);
            float r = _spShieldRadius;
            // Translucent blue aura
            Color fill = (Color){80, 170, 255, 80};
            Color line = (Color){120, 200, 255, 180};
            DrawCircle(cx, cy, r, fill);
            DrawCircleLines(cx, cy, r, line);
        }
    }
    // draw all enemies as red rectangles
    for (auto& en : _spEnemies) {
        if (auto* ep = _spWorld->get<rt::components::Position>(en.id)) {
            Rectangle rect{ep->x, ep->y, 24.f, 16.f};
            DrawRectangleRec(rect, (Color){220, 80, 80, 255});
        }
    }
    // draw bullets as yellow rectangles
    for (auto& b : _spBullets) {
        Rectangle rect{b.x, b.y, b.w, b.h};
        DrawRectangleRec(rect, (Color){240, 220, 80, 255});
    }
    // draw power-ups (green: life, blue: invincibility, purple: clear board)
    for (auto& pu : _spPowerups) {
        Color fill;
        Color line;
        if (pu.type == SpPowerupType::Invincibility) {
            fill = (Color){80, 170, 255, 220};
            line = (Color){120, 200, 255, 255};
        } else if (pu.type == SpPowerupType::ClearBoard) {
            fill = (Color){170, 80, 200, 230};
            line = (Color){210, 120, 240, 255};
        } else {
            fill = (Color){100, 220, 120, 255};
            line = (Color){60, 160, 80, 255};
        }
        DrawCircle((int)pu.x, (int)pu.y, pu.radius, fill);
        DrawCircleLines((int)pu.x, (int)pu.y, pu.radius, line);
    }

    // Draw lives bar at bottom-left: squares representing HP
    int w = GetScreenWidth();
    int h = GetScreenHeight();
    int barH = (int)(h * 0.06f);
    int margin = 8;
    DrawRectangle(0, h - barH, w, barH, (Color){0, 0, 0, 140});
    int sqSize = barH - 2 * margin;
    if (sqSize < 8) sqSize = 8;
    int gap = 6;
    int total = _maxLives;
    int startX = margin; // left-aligned
    for (int i = 0; i < total; ++i) {
        Color c = i < _playerLives ? (Color){100, 220, 120, 255} : (Color){80, 80, 80, 180};
        DrawRectangle(startX + i * (sqSize + gap), h - barH + margin, sqSize, sqSize, c);
    }

    // Draw overheat bar at center bottom: red depleting bar while firing, recharges when idle
    int barW = (int)(w * 0.35f);
    int barX = (w - barW) / 2;
    int barY = h - barH + margin;
    int barInnerH = sqSize;
    // background
    DrawRectangle(barX, barY, barW, barInnerH, (Color){60, 60, 60, 180});
    // fill
    int fillW = (int)(barW * _spHeat);
    Color fillC = _spHeat > 0.2f ? (Color){220, 90, 90, 220} : (Color){220, 40, 40, 240};
    DrawRectangle(barX, barY, fillW, barInnerH, fillC);
    // outline
    DrawRectangleLines(barX, barY, barW, barInnerH, (Color){220, 220, 220, 200});

    // Draw current score at the top-left corner
    int font = baseFontFromHeight(h);
    std::string scoreText = std::string("Score: ") + std::to_string(_score);
    DrawText(scoreText.c_str(), margin, margin, font, RAYWHITE);
}

// --- Formation spawners ---
void Screens::spScheduleNextSpawn() {
    std::uniform_real_distribution<float> dd(_spMinSpawnDelay, _spMaxSpawnDelay);
    _spNextSpawnDelay = dd(_spRng);
}
void Screens::spSpawnLine(int count, float y) {
    float startX = (float)GetScreenWidth() + 40.f;
    float spacing = 40.f;
    for (int i = 0; i < count; ++i) {
        auto e = _spWorld->create();
        float x = startX + i * spacing;
    _spWorld->emplace<rt::components::Position>((rt::ecs::Entity)e, rt::components::Position{x, y});
        _spWorld->emplace<rt::components::Enemy>((rt::ecs::Entity)e, rt::components::Enemy{});
        _spWorld->emplace<rt::components::AiController>((rt::ecs::Entity)e, rt::components::AiController{});
    _spWorld->emplace<rt::components::Size>((rt::ecs::Entity)e, rt::components::Size{24.f, 16.f});
        _spEnemies.push_back({e, SpFormationKind::Line, i, y, spacing, 0.f, 0.f, _spElapsed, (float)i * spacing, 0.f});
    }
}

void Screens::spSpawnSnake(int count, float y, float amplitude, float frequency, float spacing) {
    float startX = (float)GetScreenWidth() + 40.f;
    for (int i = 0; i < count; ++i) {
        auto e = _spWorld->create();
        float x = startX + i * spacing;
    _spWorld->emplace<rt::components::Position>((rt::ecs::Entity)e, rt::components::Position{x, y});
        _spWorld->emplace<rt::components::Enemy>((rt::ecs::Entity)e, rt::components::Enemy{});
        _spWorld->emplace<rt::components::AiController>((rt::ecs::Entity)e, rt::components::AiController{});
    _spWorld->emplace<rt::components::Size>((rt::ecs::Entity)e, rt::components::Size{24.f, 16.f});
        _spEnemies.push_back({e, SpFormationKind::Snake, i, y, spacing, amplitude, frequency, _spElapsed, (float)i * spacing, 0.f});
    }
}

void Screens::spSpawnTriangle(int rows, float y, float spacing) {
    float startX = (float)GetScreenWidth() + 40.f;
    int idx = 0;
    for (int col = 0; col < rows; ++col) {
        int count = col + 1; // 1,2,3,...
        float localX = col * spacing;
        float startY = -0.5f * (count - 1) * spacing;
        for (int r = 0; r < count; ++r) {
            float localY = startY + r * spacing;
            auto e = _spWorld->create();
            _spWorld->emplace<rt::components::Position>((rt::ecs::Entity)e, rt::components::Position{startX + localX, y + localY});
            _spWorld->emplace<rt::components::Enemy>((rt::ecs::Entity)e, rt::components::Enemy{});
            _spWorld->emplace<rt::components::AiController>((rt::ecs::Entity)e, rt::components::AiController{});
            _spWorld->emplace<rt::components::Size>((rt::ecs::Entity)e, rt::components::Size{24.f, 16.f});
            _spEnemies.push_back({e, SpFormationKind::Triangle, idx++, y, spacing, 0.f, 0.f, _spElapsed, localX, localY});
        }
    }
}

void Screens::spSpawnDiamond(int rows, float y, float spacing) {
    // Diamond shape: columns increase then decrease
    float startX = (float)GetScreenWidth() + 40.f;
    int idx = 0;
    for (int col = 0; col < rows; ++col) {
        int count = col + 1;
        float localX = col * spacing;
        float startY = -0.5f * (count - 1) * spacing;
        for (int r = 0; r < count; ++r) {
            float localY = startY + r * spacing;
            auto e = _spWorld->create();
            _spWorld->emplace<rt::components::Position>((rt::ecs::Entity)e, rt::components::Position{startX + localX, y + localY});
            _spWorld->emplace<rt::components::Enemy>((rt::ecs::Entity)e, rt::components::Enemy{});
            _spWorld->emplace<rt::components::AiController>((rt::ecs::Entity)e, rt::components::AiController{});
            _spWorld->emplace<rt::components::Size>((rt::ecs::Entity)e, rt::components::Size{24.f, 16.f});
            _spEnemies.push_back({e, SpFormationKind::Diamond, idx++, y, spacing, 0.f, 0.f, _spElapsed, localX, localY});
        }
    }
    for (int col = rows - 2; col >= 0; --col) {
        int count = col + 1;
        float localX = (2 * rows - 2 - col) * spacing; // mirror back
        float startY = -0.5f * (count - 1) * spacing;
        for (int r = 0; r < count; ++r) {
            float localY = startY + r * spacing;
            auto e = _spWorld->create();
            _spWorld->emplace<rt::components::Position>((rt::ecs::Entity)e, rt::components::Position{startX + localX, y + localY});
            _spWorld->emplace<rt::components::Enemy>((rt::ecs::Entity)e, rt::components::Enemy{});
            _spWorld->emplace<rt::components::AiController>((rt::ecs::Entity)e, rt::components::AiController{});
            _spWorld->emplace<rt::components::Size>((rt::ecs::Entity)e, rt::components::Size{24.f, 16.f});
            _spEnemies.push_back({e, SpFormationKind::Diamond, idx++, y, spacing, 0.f, 0.f, _spElapsed, localX, localY});
        }
    }
}

} } // namespace client::ui
