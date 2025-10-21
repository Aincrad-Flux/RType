#include "Screens.hpp"
#include "widgets/Button.hpp"
#include "widgets/InputBox.hpp"
#include "widgets/Title.hpp"
#include <raylib.h>
#include <cmath>

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
    _spWorld->addSystem(std::make_unique<rt::systems::AiControlSystem>());
    _spWorld->addSystem(std::make_unique<rt::systems::MovementSystem>());
    // Player entity
    auto player = _spWorld->create();
    player.add<rt::components::Position>(100.f, 100.f);
    player.add<rt::components::Controller>();
    _spPlayer = player;

    // Start with an initial simple line
    _spEnemies.clear();
    _spElapsed = 0.f;
    _spSpawnTimer = 0.f;
    _spNextFormation = 0;
    // Seed RNG with time
    std::random_device rd; _spRng.seed(rd());
    // Schedule first spawn with random delay
    spScheduleNextSpawn();
    _spInitialized = true;
}

void Screens::shutdownSingleplayerWorld() {
    _spWorld.reset();
    _spPlayer = 0;
    _spEnemies.clear();
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

    // Global timers
    _spElapsed += dt;
    _spSpawnTimer += dt;

    // Randomized spawn scheduler, with cap on active enemies
    if (_spSpawnTimer >= _spNextSpawnDelay && _spEnemies.size() < _spEnemyCap) {
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
        // Always move to the left; bias upward for the user request
        bitsAI |= kLeft;
        switch (en.kind) {
            case SpFormationKind::Snake: {
                float phase = t * en.frequency + en.index * 0.5f;
                float s = std::sin(phase);
                bitsAI |= (s > 0.f ? kUp : 0);
                // small downward occasionally to prevent sticking at top if extremely high
                if (s <= 0.f && ((en.index + (int)(t)) % 3 == 0)) bitsAI |= kDown;
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
    // draw all enemies as red rectangles
    for (auto& en : _spEnemies) {
        if (auto* ep = _spWorld->get<rt::components::Position>(en.id)) {
            Rectangle rect{ep->x, ep->y, 24.f, 16.f};
            DrawRectangleRec(rect, (Color){220, 80, 80, 255});
        }
    }
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
            _spEnemies.push_back({e, SpFormationKind::Diamond, idx++, y, spacing, 0.f, 0.f, _spElapsed, localX, localY});
        }
    }
}

} } // namespace client::ui
