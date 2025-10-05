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
        constexpr std::uint8_t kUp = 1 << 0;
        constexpr std::uint8_t kDown = 1 << 1;
        constexpr std::uint8_t kLeft = 1 << 2;
        constexpr std::uint8_t kRight = 1 << 3;
        if (inp.bits & kLeft)  vx -= inp.speed;
        if (inp.bits & kRight) vx += inp.speed;
        if (inp.bits & kUp)    vy -= inp.speed;
        if (inp.bits & kDown)  vy += inp.speed;
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

void ShootingSystem::update(rt::ecs::Registry& r, float dt) {
    // For every player with PlayerInput and Shooter, spawn bullets while holding shoot
    auto& inputs = r.storage<PlayerInput>().data();
    for (auto& [e, inp] : inputs) {
        auto* shooter = r.get<Shooter>(e);
        auto* t = r.get<Transform>(e);
        if (!shooter || !t) continue;
        shooter->cooldown -= dt;
    constexpr std::uint8_t kShoot = 1 << 4;
    bool wantShoot = (inp.bits & kShoot) != 0;
        while (wantShoot && shooter->cooldown <= 0.f) {
            shooter->cooldown += shooter->interval;
            // Spawn a bullet entity slightly ahead of the player ship
            auto b = r.create();
            float bx = t->x + 20.f; // assuming player ship width ~20
            float by = t->y + 5.f;  // center roughly
            r.emplace<Transform>(b, {bx, by});
            r.emplace<Velocity>(b, {shooter->bulletSpeed, 0.f});
            r.emplace<NetType>(b, {static_cast<rtype::net::EntityType>(3)});
            r.emplace<ColorRGBA>(b, {0xFFFF55FFu});
            r.emplace<BulletTag>(b, {});
            r.emplace<Size>(b, {6.f, 3.f});
        }
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

void DespawnOutOfBoundsSystem::update(rt::ecs::Registry& r, float dt) {
    (void)dt;
    std::vector<rt::ecs::Entity> toDestroy;
    auto& transforms = r.storage<Transform>().data();
    for (auto& [e, t] : transforms) {
        // Only consider bullets for out-of-bounds despawn to avoid killing players
        if (!r.get<BulletTag>(e)) continue;
        auto* sz = r.get<Size>(e);
        float w = sz ? sz->w : 0.f;
        float h = sz ? sz->h : 0.f;
        if (t.x + w < minX_ || t.x > maxX_ || t.y + h < minY_ || t.y > maxY_) {
            toDestroy.push_back(e);
        }
    }
    for (auto e : toDestroy) r.destroy(e);
}

// --- Spawn formations ---
rt::ecs::Entity FormationSpawnSystem::spawnSnake(rt::ecs::Registry& r, float y, int count) {
    auto origin = r.create();
    r.emplace<Transform>(origin, {980.f, y});
    r.emplace<Velocity>(origin, {-60.f, 0.f});
    r.emplace<Formation>(origin, {FormationType::Snake, -60.f, 70.f, 2.5f, 36.f, 0, 0});
    for (int i = 0; i < count; ++i) {
        auto e = r.create();
        r.emplace<Transform>(e, {980.f + i * 36.f, y});
        r.emplace<Velocity>(e, {-60.f, 0.f});
    r.emplace<NetType>(e, {static_cast<rtype::net::EntityType>(2)});
        r.emplace<ColorRGBA>(e, {0xFF5555FFu});
        r.emplace<EnemyTag>(e, {});
        r.emplace<Size>(e, {18.f, 12.f});
        r.emplace<FormationFollower>(e, {origin, static_cast<std::uint16_t>(i), i * 36.f, 0.f});
    }
    return origin;
}

rt::ecs::Entity FormationSpawnSystem::spawnLine(rt::ecs::Registry& r, float y, int count) {
    auto origin = r.create();
    r.emplace<Transform>(origin, {980.f, y});
    r.emplace<Velocity>(origin, {-60.f, 0.f});
    r.emplace<Formation>(origin, {FormationType::Line, -60.f, 0.f, 0.f, 40.f, 0, 0});
    for (int i = 0; i < count; ++i) {
        auto e = r.create();
        r.emplace<Transform>(e, {980.f + i * 40.f, y});
        r.emplace<Velocity>(e, {-60.f, 0.f});
    r.emplace<NetType>(e, {static_cast<rtype::net::EntityType>(2)});
        r.emplace<ColorRGBA>(e, {0xE06666FFu});
        r.emplace<EnemyTag>(e, {});
        r.emplace<Size>(e, {18.f, 12.f});
        r.emplace<FormationFollower>(e, {origin, static_cast<std::uint16_t>(i), i * 40.f, 0.f});
    }
    return origin;
}

rt::ecs::Entity FormationSpawnSystem::spawnGrid(rt::ecs::Registry& r, float y, int rows, int cols) {
    auto origin = r.create();
    r.emplace<Transform>(origin, {980.f, y});
    r.emplace<Velocity>(origin, {-50.f, 0.f});
    r.emplace<Formation>(origin, {FormationType::GridRect, -50.f, 0.f, 0.f, 36.f, rows, cols});
    for (int rr = 0; rr < rows; ++rr) {
        for (int cc = 0; cc < cols; ++cc) {
            int idx = rr * cols + cc;
            auto e = r.create();
            r.emplace<Transform>(e, {980.f + cc * 36.f, y + rr * 36.f});
            r.emplace<Velocity>(e, {-50.f, 0.f});
            r.emplace<NetType>(e, {static_cast<rtype::net::EntityType>(2)});
            r.emplace<ColorRGBA>(e, {0xCC4444FFu});
            r.emplace<EnemyTag>(e, {});
            r.emplace<Size>(e, {18.f, 12.f});
            r.emplace<FormationFollower>(e, {origin, static_cast<std::uint16_t>(idx), cc * 36.f, rr * 36.f});
        }
    }
    return origin;
}

rt::ecs::Entity FormationSpawnSystem::spawnTriangle(rt::ecs::Registry& r, float y, int rows) {
    auto origin = r.create();
    r.emplace<Transform>(origin, {980.f, y});
    r.emplace<Velocity>(origin, {-55.f, 0.f});
    r.emplace<Formation>(origin, {FormationType::Triangle, -55.f, 0.f, 0.f, 36.f, rows, 0});
    int idx = 0;
    // Left-pointing triangle: apex on the left, expanding columns to the right
    for (int cc = 0; cc < rows; ++cc) {
        int count = cc + 1; // number of enemies in this column
        float startY = -0.5f * (count - 1) * 36.f; // center vertically per column
        for (int rr = 0; rr < count; ++rr) {
            auto e = r.create();
            float localX = cc * 36.f;
            float localY = startY + rr * 36.f;
            r.emplace<Transform>(e, {980.f + localX, y + localY});
            r.emplace<Velocity>(e, {-55.f, 0.f});
            r.emplace<NetType>(e, {static_cast<rtype::net::EntityType>(2)});
            r.emplace<ColorRGBA>(e, {0xDD7777FFu});
            r.emplace<EnemyTag>(e, {});
            r.emplace<Size>(e, {18.f, 12.f});
            r.emplace<FormationFollower>(e, {origin, static_cast<std::uint16_t>(idx++), localX, localY});
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

void CollisionSystem::update(rt::ecs::Registry& r, float dt) {
    (void)dt;
    // Gather bullets and enemies
    std::vector<rt::ecs::Entity> bullets;
    bullets.reserve(64);
    for (auto& [e, _] : r.storage<BulletTag>().data()) bullets.push_back(e);
    std::vector<rt::ecs::Entity> enemies;
    enemies.reserve(128);
    for (auto& [e, _] : r.storage<EnemyTag>().data()) enemies.push_back(e);

    std::vector<rt::ecs::Entity> toDestroy;
    auto intersects = [&](rt::ecs::Entity a, rt::ecs::Entity b){
        auto* ta = r.get<Transform>(a); auto* sa = r.get<Size>(a);
        auto* tb = r.get<Transform>(b); auto* sb = r.get<Size>(b);
        if (!ta || !tb || !sa || !sb) return false;
        float ax2 = ta->x + sa->w, ay2 = ta->y + sa->h;
        float bx2 = tb->x + sb->w, by2 = tb->y + sb->h;
        return !(ax2 < tb->x || bx2 < ta->x || ay2 < tb->y || by2 < ta->y);
    };

    for (auto b : bullets) {
        for (auto e : enemies) {
            if (intersects(b, e)) {
                toDestroy.push_back(b);
                toDestroy.push_back(e);
                break; // one enemy per bullet
            }
        }
    }
    for (auto e : toDestroy) r.destroy(e);
}

}
