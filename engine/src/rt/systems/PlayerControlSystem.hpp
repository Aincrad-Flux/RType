#pragma once
#include "rt/ecs/System.hpp"
namespace rt::ecs { class Registry; }
namespace rt::systems {
class PlayerControlSystem : public rt::ecs::System {
  public:
    void update(rt::ecs::Registry& r, float dt) override;
};
}
