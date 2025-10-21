#include "rt/systems/MovementSystem.hpp"
#include "rt/ecs/Registry.hpp"
#include "rt/components/Position.hpp"
#include "rt/components/Velocity.hpp"

using namespace rt::systems;

void MovementSystem::update(rt::ecs::Registry& r, float dt) {
    auto& velocities = r.storage<rt::components::Velocity>().data();
    for (auto& [e, v] : velocities) {
        auto* p = r.get<rt::components::Position>(e);
        if (!p) continue;
        p->x += v.vx * dt;
        p->y += v.vy * dt;
    }
}
