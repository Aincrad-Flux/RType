#include "rt/systems/PlayerControlSystem.hpp"
#include "rt/ecs/Registry.hpp"
#include "rt/components/Controller.hpp"
#include "rt/components/Velocity.hpp"

using namespace rt::systems;

void PlayerControlSystem::update(rt::ecs::Registry& r, float dt) {
    (void)dt; // input processing sets velocity instantly; integration is in MovementSystem
    constexpr std::uint8_t kUp = 1 << 0;
    constexpr std::uint8_t kDown = 1 << 1;
    constexpr std::uint8_t kLeft = 1 << 2;
    constexpr std::uint8_t kRight = 1 << 3;

    auto& ctrls = r.storage<rt::components::Controller>().data();
    for (auto& [e, c] : ctrls) {
        float vx = 0.f, vy = 0.f;
        if (c.bits & kLeft)  vx -= c.speed;
        if (c.bits & kRight) vx += c.speed;
        if (c.bits & kUp)    vy -= c.speed;
        if (c.bits & kDown)  vy += c.speed;
        auto* v = r.get<rt::components::Velocity>(e);
        if (!v) { r.emplace<rt::components::Velocity>(e, {vx, vy}); }
        else { v->vx = vx; v->vy = vy; }
    }
}
