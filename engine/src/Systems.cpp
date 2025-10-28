#include <cmath>
#include <algorithm>
#include <random>
#include <limits>
#include <vector>
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
    constexpr std::uint8_t kShoot = 1 << 4;
    for (auto& [e, inp] : inputs) {
        auto* shooter = r.get<Shooter>(e);
        auto* t = r.get<Transform>(e);
        if (!shooter || !t) continue;
        shooter->cooldown -= dt;
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
            r.emplace<BulletTag>(b, {BulletFaction::Player});
            r.emplace<BulletOwner>(b, {e});
            r.emplace<Size>(b, {6.f, 3.f});
        }
    }
}

void ChargeShootingSystem::update(rt::ecs::Registry& r, float dt) {
    (void)dt;
    constexpr std::uint8_t kCharge = 1 << 5; // must match Protocol InputCharge
    auto& inputs = r.storage<PlayerInput>().data();
    for (auto& [e, inp] : inputs) {
        auto* t = r.get<Transform>(e);
        if (!t) continue;
        auto* cg = r.get<ChargeGun>(e);
        if (!cg) continue; // optional feature per player
        bool holding = (inp.bits & kCharge) != 0;
        if (holding) {
            cg->charge = std::min(cg->maxCharge, cg->charge + dt);
        } else {
            if (cg->charge > 0.05f) {
                // Fire beam once, thickness based on charge
                float thickness = 8.f + (cg->charge / cg->maxCharge) * 44.f; // 8..52
                auto b = r.create();
                float bx = t->x + 10.f; // from player
                float by = t->y + 6.f;  // centered on player
                r.emplace<Transform>(b, {bx, by - thickness * 0.5f});
                // Beam is instant; represent as a wide, slow-moving rectangle that lives one tick
                r.emplace<Velocity>(b, {600.f, 0.f});
                r.emplace<NetType>(b, {static_cast<rtype::net::EntityType>(3)});
                r.emplace<ColorRGBA>(b, {0x77CCFFFFu});
                r.emplace<BulletTag>(b, {BulletFaction::Player});
                r.emplace<BulletOwner>(b, {e});
                r.emplace<Size>(b, {700.f, thickness});
                r.emplace<BeamTag>(b, {});
                // Reset charge
                cg->charge = 0.f;
            }
        }
    }
}

// Enemy shooting towards nearest player with variable accuracy
void EnemyShootingSystem::update(rt::ecs::Registry& r, float dt) {
    // Build a list of players
    std::vector<rt::ecs::Entity> players;
    for (auto& [e, nt] : r.storage<NetType>().data()) {
        if (nt.type == rtype::net::EntityType::Player) players.push_back(e);
    }
    if (players.empty()) return;

    // Update each enemy with EnemyShooter
    auto& shooters = r.storage<EnemyShooter>().data();
    for (auto& [e, es] : shooters) {
        es.cooldown -= dt;
        if (es.cooldown > 0.f) continue;
        auto* t = r.get<Transform>(e);
        if (!t) continue;
        // Find nearest player
        rt::ecs::Entity best = players[0];
        float bestDist2 = std::numeric_limits<float>::infinity();
        for (auto p : players) {
            auto* pt = r.get<Transform>(p);
            if (!pt) continue;
            float dx = pt->x - t->x;
            float dy = pt->y - t->y;
            float d2 = dx*dx + dy*dy;
            if (d2 < bestDist2) { bestDist2 = d2; best = p; }
        }
        auto* pt = r.get<Transform>(best);
        if (!pt) continue;
        // Compute direction with inaccuracy
        float dx = pt->x - t->x;
        float dy = pt->y - t->y;
        float len = std::sqrt(dx*dx + dy*dy);
        if (len < 1e-3f) { dx = 1.f; dy = 0.f; len = 1.f; }
        dx /= len; dy /= len;
        // accuracy in [0.5, 0.8] applied as blend towards ideal direction, with random perpendicular jitter
        float acc = std::clamp(es.accuracy, 0.5f, 0.8f);
        // random angle offset up to maxAngle where lower accuracy => larger offset
        float maxAngle = (1.f - acc) * 0.5f; // radians up to ~0.25 rad ~ 14deg when acc=0.5
        std::uniform_real_distribution<float> ang(-maxAngle, maxAngle);
        float a = ang(rng_);
        float cs = std::cos(a), sn = std::sin(a);
        float dirx = dx * cs - dy * sn;
        float diry = dx * sn + dy * cs;
        // Spawn bullet
        auto b = r.create();
        float bx = t->x - 10.f; // from enemy front
        float by = t->y + 6.f;
        r.emplace<Transform>(b, {bx, by});
        r.emplace<Velocity>(b, {dirx * es.bulletSpeed, diry * es.bulletSpeed});
        r.emplace<NetType>(b, {static_cast<rtype::net::EntityType>(3)});
        r.emplace<ColorRGBA>(b, {0xFFAA00FFu});
        r.emplace<BulletTag>(b, {BulletFaction::Enemy});
        r.emplace<Size>(b, {6.f, 3.f});
        es.cooldown += es.interval;
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
        // Clamp follower vertical position inside playable area so enemies don't overlap HUD or leave screen
        // World vertical bounds are defined [kTopMargin, kWorldH - kBottomMargin - entityHeight]
        constexpr float kWorldH = 600.f;        // server world height used across systems
        constexpr float kTopMargin = 56.f;      // reserve top HUD area (~name+lvl bar)
        constexpr float kBottomMargin = 10.f;   // small safety margin at bottom
        if (auto* sz = r.get<Size>(e)) {
            float maxY = kWorldH - kBottomMargin - std::max(0.f, sz->h);
            y = std::clamp(y, kTopMargin, maxY);
        } else {
            // If size unknown, still keep roughly within screen
            y = std::clamp(y, kTopMargin, kWorldH - kBottomMargin);
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
        r.emplace<Size>(e, {27.f, 18.f});
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
        r.emplace<Size>(e, {27.f, 18.f});
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
            r.emplace<Size>(e, {27.f, 18.f});
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
            r.emplace<Size>(e, {27.f, 18.f});
            r.emplace<FormationFollower>(e, {origin, static_cast<std::uint16_t>(idx++), localX, localY});
        }
    }
    return origin;
}

// Big enemies that also shoot at players
rt::ecs::Entity FormationSpawnSystem::spawnBigShooters(rt::ecs::Registry& r, float y, int count) {
    auto origin = r.create();
    r.emplace<Transform>(origin, {980.f, y});
    r.emplace<Velocity>(origin, {-40.f, 0.f});
    r.emplace<Formation>(origin, {FormationType::Line, -40.f, 0.f, 0.f, 64.f, 0, 0});
    std::uniform_real_distribution<float> accd(0.5f, 0.8f);
    for (int i = 0; i < count; ++i) {
        auto e = r.create();
        float localX = i * 64.f;
        r.emplace<Transform>(e, {980.f + localX, y});
        r.emplace<Velocity>(e, {-40.f, 0.f});
        r.emplace<NetType>(e, {static_cast<rtype::net::EntityType>(2)});
        r.emplace<ColorRGBA>(e, {0xAA3333FFu});
        r.emplace<EnemyTag>(e, {});
        r.emplace<Size>(e, {28.f, 20.f});
        r.emplace<FormationFollower>(e, {origin, static_cast<std::uint16_t>(i), localX, 0.f});
        r.emplace<EnemyShooter>(e, {0.f, 1.2f, 240.f, accd(rng_)});
    }
    return origin;
}

void FormationSpawnSystem::update(rt::ecs::Registry& r, float dt) {
    // Do not spawn regular waves if a boss is active/present
    for (auto& [e, _] : r.storage<BossTag>().data()) { (void)e; return; }
    timer_ += dt;
    if (timer_ < 3.0f) return;
    timer_ = 0.f;
    // Limit to at most two active formations (origins)
    int activeFormations = 0;
    for (auto& [e, _] : r.storage<Formation>().data()) { (void)e; ++activeFormations; }
    if (activeFormations >= 2) return;
    // World and margins
    constexpr float kWorldH = 600.f;
    constexpr float kTopMargin = 56.f;
    constexpr float kBottomMargin = 10.f;
    constexpr float kEnemyH = 18.f; // default enemy sprite height (increased)
    constexpr float kSpacing = 36.f;
    std::uniform_int_distribution<int> pick(0, 4);
    int k = pick(rng_);
    float y = kTopMargin + 80.f; // fallback
    switch (k) {
        case 0: {
            // Snake: need room for sine amplitude on both sides
            float amplitude = 70.f;
            float minY = kTopMargin + amplitude;
            float maxY = kWorldH - kBottomMargin - amplitude - kEnemyH;
            if (minY > maxY) std::swap(minY, maxY);
            std::uniform_real_distribution<float> ydist(minY, maxY);
            y = ydist(rng_);
            spawnSnake(r, y, 6);
            break;
        }
        case 1: {
            // Line: single row
            float minY = kTopMargin;
            float maxY = kWorldH - kBottomMargin - kEnemyH;
            std::uniform_real_distribution<float> ydist(minY, maxY);
            y = ydist(rng_);
            spawnLine(r, y, 8);
            break;
        }
        case 2: {
            // Grid: rows x cols, vertical extent = (rows-1)*spacing + enemyH
            int rows = 3, cols = 5;
            float extent = (rows - 1) * kSpacing + kEnemyH;
            float minY = kTopMargin;
            float maxY = kWorldH - kBottomMargin - extent;
            if (minY > maxY) std::swap(minY, maxY);
            std::uniform_real_distribution<float> ydist(minY, maxY);
            y = ydist(rng_);
            spawnGrid(r, y, rows, cols);
            break;
        }
        case 3: {
            // Triangle: vertical half-extent = 0.5*(rows-1)*spacing
            int rows = 5;
            float half = 0.5f * (rows - 1) * kSpacing;
            float minY = kTopMargin + half;
            float maxY = kWorldH - kBottomMargin - half - kEnemyH;
            if (minY > maxY) std::swap(minY, maxY);
            std::uniform_real_distribution<float> ydist(minY, maxY);
            y = ydist(rng_);
            spawnTriangle(r, y, rows);
            break;
        }
        case 4: {
            // Big shooters line, fewer units and larger size
            float minY = kTopMargin;
            float maxY = kWorldH - kBottomMargin - 20.f; // account for big enemy height
            std::uniform_real_distribution<float> ydist(minY, maxY);
            y = ydist(rng_);
            spawnBigShooters(r, y, 3);
            break;
        }
        default: spawnLine(r, y, 6); break;
    }
}

void CollisionSystem::update(rt::ecs::Registry& r, float dt) {
    (void)dt;
    // Gather bullets
    std::vector<rt::ecs::Entity> bullets;
    bullets.reserve(128);
    for (auto& [e, _] : r.storage<BulletTag>().data()) bullets.push_back(e);

    std::vector<rt::ecs::Entity> toDestroy;
    auto intersects = [&](rt::ecs::Entity a, rt::ecs::Entity b){
        auto* ta = r.get<Transform>(a); auto* sa = r.get<Size>(a);
        auto* tb = r.get<Transform>(b); auto* sb = r.get<Size>(b);
        if (!ta || !tb || !sa || !sb) return false;
        float ax2 = ta->x + sa->w, ay2 = ta->y + sa->h;
        float bx2 = tb->x + sb->w, by2 = tb->y + sb->h;
        return !(ax2 < tb->x || bx2 < ta->x || ay2 < tb->y || by2 < ta->y);
    };

    // Collide bullets with appropriate targets
    for (auto b : bullets) {
        auto* bt = r.get<BulletTag>(b);
        if (!bt) continue;
        bool isBeam = r.get<BeamTag>(b) != nullptr;
        if (bt->faction == BulletFaction::Player) {
            // hit enemies
            for (auto& [e, _] : r.storage<EnemyTag>().data()) {
                if (!intersects(b, e)) continue;
                // If this enemy is a boss, decrement HP instead of immediate death
                if (auto* boss = r.get<BossTag>(e)) {
                    // apply damage
                    if (boss->hp > 0) boss->hp -= 1;
                    // Destroy bullet unless it's a beam
                    if (!isBeam) toDestroy.push_back(b);
                    // If dead now, destroy boss and award big score
                    if (boss->hp <= 0) {
                        // award score to bullet owner if any
                        if (auto* bo = r.get<BulletOwner>(b)) if (auto* sc = r.get<Score>(bo->owner)) sc->value += 1000;
                        toDestroy.push_back(e);
                    }
                    // For bosses, do not process further targets for this bullet if it's not a beam
                    if (!isBeam) break;
                    else continue;
                }
                // Normal enemy: award score and destroy
                if (auto* bo = r.get<BulletOwner>(b)) {
                    if (auto* sc = r.get<Score>(bo->owner)) {
                        sc->value += 50;
                    }
                }
                if (!isBeam) toDestroy.push_back(b);
                toDestroy.push_back(e);
                if (!isBeam) break; // stop after first hit for regular bullets
            }
        } else {
            // enemy bullets hit players
            for (auto& [e, _] : r.storage<PlayerInput>().data()) {
                // players have PlayerInput component
                if (!intersects(b, e)) continue;
                // If player is currently invincible, ignore this hit (but still destroy bullet)
                if (auto* inv = r.get<Invincible>(e)) {
                    if (inv->timeLeft > 0.f) { toDestroy.push_back(b); break; }
                }
                // Mark player as hit; server will process lives decrement
                if (auto* hf = r.get<HitFlag>(e)) {
                    hf->value = true;
                } else {
                    r.emplace<HitFlag>(e, {true});
                }
                // Apply a brief invincibility to prevent immediate re-hits
                if (auto* inv = r.get<Invincible>(e)) {
                    inv->timeLeft = std::max(inv->timeLeft, 1.0f);
                } else {
                    r.emplace<Invincible>(e, {1.0f});
                }
                toDestroy.push_back(b);
                break;
            }
        }
    }
    for (auto e : toDestroy) r.destroy(e);
}

// Decrement invincibility timers each frame
void InvincibilitySystem::update(rt::ecs::Registry& r, float dt) {
    // Tick invincibility
    auto& invs = r.storage<Invincible>().data();
    for (auto& [e, inv] : invs) {
        inv.timeLeft -= dt;
        if (inv.timeLeft <= 0.f) {
            inv.timeLeft = 0.f;
        }
    }
}

// Spawn the boss when any player's score reaches threshold. Only one boss per run.
void BossSpawnSystem::update(rt::ecs::Registry& r, float dt) {
    (void)dt;
    // If a boss already exists, nothing to do
    for (auto& [e, _] : r.storage<BossTag>().data()) { (void)e; spawned_ = true; return; }
    if (spawned_) return;
    // Check any player's score
    int bestScore = 0;
    for (auto& [e, sc] : r.storage<Score>().data()) { (void)e; bestScore = std::max(bestScore, sc.value); }
    if (bestScore < threshold_) return;
    // Spawn boss entity
    // World/margins consistent with formation logic
    constexpr float kWorldH = 600.f;
    constexpr float kTopMargin = 56.f;
    constexpr float kBottomMargin = 10.f;
    // Boss size (bigger than enemies)
    float bw = 160.f;
    float bh = 120.f;
    float yMin = kTopMargin;
    float yMax = kWorldH - kBottomMargin - bh;
    if (yMax < yMin) yMax = yMin;
    float by = 0.5f * (yMin + yMax);
    // Spawn slightly offscreen to the right
    auto e = r.create();
    r.emplace<Transform>(e, Transform{980.f + 60.f, by});
    r.emplace<Velocity>(e, Velocity{-60.f, 0.f});
    r.emplace<Size>(e, Size{bw, bh});
    r.emplace<ColorRGBA>(e, ColorRGBA{0x9646B4FFu}); // purple
    r.emplace<NetType>(e, NetType{static_cast<rtype::net::EntityType>(2)}); // treat as Enemy for net
    r.emplace<EnemyTag>(e, EnemyTag{});
    BossTag boss{};
    boss.maxHp = 50;
    boss.hp = boss.maxHp;
    boss.rightMargin = 20.f;
    // Assume server world width ~ 960; stop so boss fully visible
    float worldW = 960.f;
    boss.stopX = worldW - boss.rightMargin - bw;
    boss.atStop = false;
    boss.dirDown = true;
    boss.speedX = -60.f;
    boss.speedY = 100.f;
    r.emplace<BossTag>(e, boss);
    spawned_ = true;
}

// Control boss movement: slide to stopX, then bounce vertically within world bounds
void BossSystem::update(rt::ecs::Registry& r, float dt) {
    constexpr float kWorldH = 600.f;
    constexpr float kTopMargin = 56.f;
    constexpr float kBottomMargin = 10.f;
    for (auto& [e, boss] : r.storage<BossTag>().data()) {
        auto* t = r.get<Transform>(e);
        auto* v = r.get<Velocity>(e);
        auto* s = r.get<Size>(e);
        if (!t || !s) continue;
        if (!v) { r.emplace<Velocity>(e, Velocity{0.f, 0.f}); v = r.get<Velocity>(e); }
        float minY = kTopMargin;
        float maxY = kWorldH - kBottomMargin - s->h;
        if (!boss.atStop) {
            // Move left towards stopX
            if (t->x > boss.stopX) {
                v->vx = boss.speedX;
            } else {
                t->x = boss.stopX;
                v->vx = 0.f;
                boss.atStop = true;
            }
            // While approaching, keep inside vertical bounds
            if (t->y < minY) t->y = minY;
            if (t->y > maxY) t->y = maxY;
            v->vy = 0.f;
        } else {
            // Vertical patrol
            v->vx = 0.f;
            if (boss.dirDown) {
                v->vy = std::abs(boss.speedY);
                if (t->y >= maxY) boss.dirDown = false;
            } else {
                v->vy = -std::abs(boss.speedY);
                if (t->y <= minY) boss.dirDown = true;
            }
            // Clamp to ensure we don't overshoot bounds
            if (t->y < minY) t->y = minY;
            if (t->y > maxY) t->y = maxY;
        }
    }
}
