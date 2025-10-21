#include "rt/systems/CollisionSystem.hpp"
#include "rt/ecs/Registry.hpp"
#include "rt/components/Position.hpp"
#include "rt/components/Size.hpp"
#include "rt/components/Player.hpp"
#include "rt/components/Enemy.hpp"
#include "rt/components/Collided.hpp"

using namespace rt::systems;

static inline bool aabbOverlap(float ax, float ay, float aw, float ah,
                               float bx, float by, float bw, float bh) {
    return !(ax + aw < bx || bx + bw < ax || ay + ah < by || by + bh < ay);
}

void CollisionSystem::update(rt::ecs::Registry& r, float dt) {
    (void)dt;
    // Build lists of players and enemies that have Position+Size
    std::vector<rt::ecs::Entity> players;
    for (auto& [e, _] : r.storage<rt::components::Player>().data()) {
        auto* p = r.get<rt::components::Position>(e);
        auto* s = r.get<rt::components::Size>(e);
        if (p && s) players.push_back(e);
    }
    if (players.empty()) return;
    for (auto& [en, _] : r.storage<rt::components::Enemy>().data()) {
        auto* ep = r.get<rt::components::Position>(en);
        auto* es = r.get<rt::components::Size>(en);
        if (!ep || !es) continue;
        for (auto pl : players) {
            auto* pp = r.get<rt::components::Position>(pl);
            auto* ps = r.get<rt::components::Size>(pl);
            if (!pp || !ps) continue;
            if (aabbOverlap(pp->x, pp->y, ps->w, ps->h, ep->x, ep->y, es->w, es->h)) {
                // mark collision on player (set true if exists, else add)
                if (auto* col = r.get<rt::components::Collided>(pl)) {
                    col->value = true;
                } else {
                    r.emplace<rt::components::Collided>(pl, {});
                }
            }
        }
    }
}
