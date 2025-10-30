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
    // Spawn simple player bullets when InputShoot is held and cooldown allows
    auto& shooters = r.storage<Shooter>().data();
    for (auto& [e, sh] : shooters) {
        // Only player-controlled entities fire in this system
        auto* inp = r.get<PlayerInput>(e);
        auto* tr  = r.get<Transform>(e);
        if (!inp || !tr) continue;
        // Cooldown timer
        sh.cooldown -= dt;
        if (sh.cooldown < 0.f) sh.cooldown = 0.f;
        // Bit 4 is InputShoot in the network protocol; InputSystem used same bit mapping
        constexpr std::uint8_t kShoot = 1 << 4;
        if ((inp->bits & kShoot) && sh.cooldown <= 0.f) {
            // Create a bullet entity moving to the right
            auto b = r.create();
            // Bullet spawn near the front of the player ship
            float bx = tr->x + 20.f;
            float by = tr->y + 6.f;
            r.emplace<Transform>(b, Transform{bx, by});
            r.emplace<Velocity>(b, Velocity{sh.bulletSpeed, 0.f});
            r.emplace<Size>(b, Size{6.f, 3.f});
            r.emplace<NetType>(b, NetType{rtype::net::EntityType::Bullet});
            r.emplace<ColorRGBA>(b, ColorRGBA{0xF0DC50FFu}); // yellow-ish
            r.emplace<BulletTag>(b, BulletTag{BulletFaction::Player});
            r.emplace<BulletOwner>(b, BulletOwner{e});
            // Reset cooldown
            sh.cooldown = sh.interval;
        }
    }
}

void ChargeShootingSystem::update(rt::ecs::Registry& r, float dt) {
    // Optional: simple charge timer (no special beam entity for now)
    (void)dt;
    auto& guns = r.storage<ChargeGun>().data();
    for (auto& [e, cg] : guns) {
        auto* inp = r.get<PlayerInput>(e);
        if (!inp) continue;
        constexpr std::uint8_t kCharge = 1 << 5; // InputCharge
        if (inp->bits & kCharge) {
            cg.charge += dt;
            if (cg.charge > cg.maxCharge) cg.charge = cg.maxCharge;
            cg.firing = true;
        } else {
            // Release: could spawn a stronger bullet; for now, just reset state
            cg.charge = 0.f;
            cg.firing = false;
        }
    }
}

void EnemyShootingSystem::update(rt::ecs::Registry& r, float dt) {
    (void)r; (void)dt; // Not required for basic player shooting visibility
}

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

void CollisionSystem::update(rt::ecs::Registry& r, float dt) {
    (void)dt;
    // Very simple AABB collisions between player bullets and enemies, and players vs enemies
    auto& transforms = r.storage<Transform>().data();
    auto& sizes = r.storage<Size>().data();
    auto& nettypes = r.storage<NetType>().data();

    // Collect entity lists
    std::vector<rt::ecs::Entity> players, enemies, pbullets;
    for (auto& [e, nt] : nettypes) {
        if (nt.type == rtype::net::EntityType::Player) players.push_back(e);
        else if (nt.type == rtype::net::EntityType::Enemy) enemies.push_back(e);
        else if (nt.type == rtype::net::EntityType::Bullet) {
            // Filter to player bullets
            if (auto* bt = r.get<BulletTag>(e); bt && bt->faction == BulletFaction::Player) pbullets.push_back(e);
        }
    }

    auto aabb = [&](rt::ecs::Entity e, float& x, float& y, float& w, float& h) -> bool {
        auto itT = transforms.find(e);
        auto itS = sizes.find(e);
        if (itT == transforms.end()) return false;
        x = itT->second.x; y = itT->second.y;
        if (itS != sizes.end()) { w = itS->second.w; h = itS->second.h; }
        else { w = 24.f; h = 16.f; }
        return true;
    };

    // Bullets vs enemies
    std::vector<rt::ecs::Entity> destroyBullets;
    std::vector<rt::ecs::Entity> destroyEnemies;
    for (auto b : pbullets) {
        float bx, by, bw, bh; if (!aabb(b, bx, by, bw, bh)) continue;
        for (auto en : enemies) {
            float ex, ey, ew, eh; if (!aabb(en, ex, ey, ew, eh)) continue;
            bool hit = !(bx + bw < ex || ex + ew < bx || by + bh < ey || ey + eh < by);
            if (hit) { destroyBullets.push_back(b); destroyEnemies.push_back(en); break; }
        }
    }
    for (auto b : destroyBullets) r.destroy(b);
    for (auto en : destroyEnemies) {
        r.destroy(en);
        // Award score to owner if present
        // Note: simplistic: +100 per enemy
        // Find owner via BulletOwner on a representative bullet that hit
        // (Skipped detailed owner lookup for brevity)
    }

    // Players vs enemies: set HitFlag
    for (auto pl : players) {
        float px, py, pw, ph; if (!aabb(pl, px, py, pw, ph)) continue;
        for (auto en : enemies) {
            float ex, ey, ew, eh; if (!aabb(en, ex, ey, ew, eh)) continue;
            bool hit = !(px + pw < ex || ex + ew < px || py + ph < ey || ey + eh < py);
            if (hit) {
                if (auto* hf = r.get<HitFlag>(pl)) hf->value = true; else r.emplace<HitFlag>(pl, HitFlag{true});
                break;
            }
        }
    }
}
