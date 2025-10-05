#include <cmath>
#include <algorithm>
#include "rt/game/Systems.hpp"

namespace rt::game {

void InputSystem::update(rt::ecs::Registry& r, float dt) {
    (void)dt;
    auto& inputs = r.storage<PlayerInput>().data();
    for (auto& [e, inp] : inputs) {
        auto* t = r.get<Transform>(e);
        if (!t) continue;
        float vx = 0.f, vy = 0.f;
        if (inp.bits & rtype::net::InputLeft)  vx -= inp.speed;
        if (inp.bits & rtype::net::InputRight) vx += inp.speed;
        if (inp.bits & rtype::net::InputUp)    vy -= inp.speed;
        if (inp.bits & rtype::net::InputDown)  vy += inp.speed;
        // Directly integrate on transform (simple for now)
        t->x += vx * dt;
        t->y += vy * dt;
    }
}

void MovementSystem::update(rt::ecs::Registry& r, float dt) {
    auto& vels = r.storage<Velocity>().data();
    for (auto& [e, v] : vels) {
        auto* t = r.get<Transform>(e);
        if (!t) continue;
        t->x += v.vx * dt;
        t->y += v.vy * dt;
    }
}

void FormationSystem::update(rt::ecs::Registry& r, float dt) {
    (void)dt;
    float time = t_ ? *t_ : 0.f;
    // Move formation origins and compute follower world positions
    auto& forms = r.storage<Formation>().data();
    for (auto& [origin, f] : forms) {
        // origin moves using its velocity if present
        if (auto* v = r.get<Velocity>(origin)) {
            if (auto* t = r.get<Transform>(origin)) {
                t->x += v->vx * dt;
                t->y += v->vy * dt;
            }
        }
    }
    // Update followers
    auto& followers = r.storage<FormationFollower>().data();
    for (auto& [e, ff] : followers) {
        auto* t = r.get<Transform>(e);
        if (!t) continue;
        auto* fo = r.get<Formation>(ff.formation);
        auto* tor = r.get<Transform>(ff.formation);
        if (!fo || !tor) continue;
        float x = tor->x + ff.localX;
        float y = tor->y + ff.localY;
        switch (fo->type) {
            case FormationType::Snake: {
                float phase = time * fo->frequency + ff.index * 0.6f;
                y += std::sin(phase) * fo->amplitude;
                break;
            }
            case FormationType::Line:
            case FormationType::GridRect:
            case FormationType::Triangle:
            case FormationType::None:
            default:
                break;
        }
        t->x = x; t->y = y;
        // inherit velocity for serialization
        if (auto* v = r.get<Velocity>(e)) v->vx = -std::abs(fo->speedX);
    }
}

void DespawnOffscreenSystem::update(rt::ecs::Registry& r, float dt) {
    (void)dt;
    std::vector<rt::ecs::Entity> toDestroy;
    auto& transforms = r.storage<Transform>().data();
    for (auto& [e, t] : transforms) {
        if (t.x < minX_) {
            toDestroy.push_back(e);
        }
    }
    for (auto e : toDestroy) r.destroy(e);
}

// --- Spawn formations ---
rt::ecs::Entity FormationSpawnSystem::spawnSnake(rt::ecs::Registry& r, float y, int count) {
    auto origin = r.create();
    r.emplace<Transform>(origin, {980.f, y});
    r.emplace<Velocity>(origin, {-90.f, 0.f});
    r.emplace<Formation>(origin, {FormationType::Snake, -90.f, 70.f, 2.5f, 36.f, 0, 0});
    for (int i = 0; i < count; ++i) {
        auto e = r.create();
        r.emplace<Transform>(e, {980.f + i * 36.f, y});
        r.emplace<Velocity>(e, {-90.f, 0.f});
        r.emplace<NetType>(e, {rtype::net::EntityType::Enemy});
        r.emplace<ColorRGBA>(e, {0xFF5555FFu});
        r.emplace<EnemyTag>(e, {});
        r.emplace<FormationFollower>(e, {origin, static_cast<std::uint16_t>(i), i * 36.f, 0.f});
    }
    return origin;
}

rt::ecs::Entity FormationSpawnSystem::spawnLine(rt::ecs::Registry& r, float y, int count) {
    auto origin = r.create();
    r.emplace<Transform>(origin, {980.f, y});
    r.emplace<Velocity>(origin, {-100.f, 0.f});
    r.emplace<Formation>(origin, {FormationType::Line, -100.f, 0.f, 0.f, 40.f, 0, 0});
    for (int i = 0; i < count; ++i) {
        auto e = r.create();
        r.emplace<Transform>(e, {980.f + i * 40.f, y});
        r.emplace<Velocity>(e, {-100.f, 0.f});
        r.emplace<NetType>(e, {rtype::net::EntityType::Enemy});
        r.emplace<ColorRGBA>(e, {0xE06666FFu});
        r.emplace<EnemyTag>(e, {});
        r.emplace<FormationFollower>(e, {origin, static_cast<std::uint16_t>(i), i * 40.f, 0.f});
    }
    return origin;
}

rt::ecs::Entity FormationSpawnSystem::spawnGrid(rt::ecs::Registry& r, float y, int rows, int cols) {
    auto origin = r.create();
    r.emplace<Transform>(origin, {980.f, y});
    r.emplace<Velocity>(origin, {-70.f, 0.f});
    r.emplace<Formation>(origin, {FormationType::GridRect, -70.f, 0.f, 0.f, 36.f, rows, cols});
    for (int rr = 0; rr < rows; ++rr) {
        for (int cc = 0; cc < cols; ++cc) {
            int idx = rr * cols + cc;
            auto e = r.create();
            r.emplace<Transform>(e, {980.f + cc * 36.f, y + rr * 36.f});
            r.emplace<Velocity>(e, {-70.f, 0.f});
            r.emplace<NetType>(e, {rtype::net::EntityType::Enemy});
            r.emplace<ColorRGBA>(e, {0xCC4444FFu});
            r.emplace<EnemyTag>(e, {});
            r.emplace<FormationFollower>(e, {origin, static_cast<std::uint16_t>(idx), cc * 36.f, rr * 36.f});
        }
    }
    return origin;
}

rt::ecs::Entity FormationSpawnSystem::spawnTriangle(rt::ecs::Registry& r, float y, int rows) {
    auto origin = r.create();
    r.emplace<Transform>(origin, {980.f, y});
    r.emplace<Velocity>(origin, {-80.f, 0.f});
    r.emplace<Formation>(origin, {FormationType::Triangle, -80.f, 0.f, 0.f, 36.f, rows, 0});
    int idx = 0;
    for (int rr = 0; rr < rows; ++rr) {
        int cols = rr + 1;
        float startX = -0.5f * (cols - 1) * 36.f;
        for (int cc = 0; cc < cols; ++cc) {
            auto e = r.create();
            r.emplace<Transform>(e, {980.f + startX + cc * 36.f, y + rr * 36.f});
            r.emplace<Velocity>(e, {-80.f, 0.f});
            r.emplace<NetType>(e, {rtype::net::EntityType::Enemy});
            r.emplace<ColorRGBA>(e, {0xDD7777FFu});
            r.emplace<EnemyTag>(e, {});
            r.emplace<FormationFollower>(e, {origin, static_cast<std::uint16_t>(idx++), startX + cc * 36.f, rr * 36.f});
        }
    }
    return origin;
}

void FormationSpawnSystem::update(rt::ecs::Registry& r, float dt) {
    timer_ += dt;
    if (timer_ < 3.0f) return;
    timer_ = 0.f;
    std::uniform_real_distribution<float> ydist(60.f, 520.f);
    std::uniform_int_distribution<int> pick(0, 3);
    int k = pick(rng_);
    float y = ydist(rng_);
    switch (k) {
        case 0: spawnSnake(r, y, 6); break;
        case 1: spawnLine(r, y, 8); break;
        case 2: spawnGrid(r, y, 3, 5); break;
        case 3: spawnTriangle(r, y, 5); break;
        default: spawnLine(r, y, 6); break;
    }
}

}
