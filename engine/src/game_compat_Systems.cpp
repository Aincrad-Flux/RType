#include <cmath>
#include <algorithm>
#include <vector>
#include <random>
#include "rt/game/Systems.hpp"

using namespace rt::game;

void InputSystem::update(rt::ecs::Registry& r, float dt) {
    (void)dt;
    auto& inputs = r.storage<PlayerInput>().data();
    for (auto& [e, inp] : inputs) {
        auto* t = r.get<Transform>(e);
        if (!t) continue;
        float vx = 0.f, vy = 0.f;
        constexpr std::uint8_t kUp = 1 << 0;
        constexpr std::uint8_t kDown = 1 << 1;
        constexpr std::uint8_t kLeft = 1 << 2;
        constexpr std::uint8_t kRight = 1 << 3;
        if (inp.bits & kLeft)  vx -= inp.speed;
        if (inp.bits & kRight) vx += inp.speed;
        if (inp.bits & kUp)    vy -= inp.speed;
        if (inp.bits & kDown)  vy += inp.speed;
        t->x += vx * dt;
        t->y += vy * dt;
    }
}

void MovementSystem::update(rt::ecs::Registry& r, float dt) {
    auto& vels = r.storage<Velocity>().data();
    for (auto& [e, v] : vels) {
        if (auto* t = r.get<Transform>(e)) { t->x += v.vx * dt; t->y += v.vy * dt; }
    }
}

void ShootingSystem::update(rt::ecs::Registry& r, float dt) {
    // Minimal no-op to satisfy server linkage; bullets are not needed for compile.
    (void)r; (void)dt;
}

void ChargeShootingSystem::update(rt::ecs::Registry& r, float dt) {
    (void)r; (void)dt;
}

void EnemyShootingSystem::update(rt::ecs::Registry& r, float dt) { (void)r; (void)dt; }

void FormationSystem::update(rt::ecs::Registry& r, float dt) { (void)r; (void)dt; }

void DespawnOffscreenSystem::update(rt::ecs::Registry& r, float dt) {
    (void)dt;
    std::vector<rt::ecs::Entity> toDestroy;
    for (auto& [e, t] : r.storage<Transform>().data()) if (t.x < minX_) toDestroy.push_back(e);
    for (auto e : toDestroy) r.destroy(e);
}

void DespawnOutOfBoundsSystem::update(rt::ecs::Registry& r, float dt) {
    (void)dt;
    std::vector<rt::ecs::Entity> toDestroy;
    for (auto& [e, t] : r.storage<Transform>().data()) {
        if (t.x < minX_ || t.x > maxX_ || t.y < minY_ || t.y > maxY_) toDestroy.push_back(e);
    }
    for (auto e : toDestroy) r.destroy(e);
}

void InvincibilitySystem::update(rt::ecs::Registry& r, float dt) {
    auto& invs = r.storage<Invincible>().data();
    for (auto& [e, inv] : invs) { (void)e; inv.timeLeft = std::max(0.f, inv.timeLeft - dt); }
}

void FormationSpawnSystem::update(rt::ecs::Registry& r, float dt) { (void)r; (void)dt; }

void CollisionSystem::update(rt::ecs::Registry& r, float dt) { (void)r; (void)dt; }
