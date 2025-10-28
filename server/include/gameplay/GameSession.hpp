#pragma once
#include "rt/ecs/Registry.hpp"
#include <random>

namespace rt { namespace game { class FormationSpawnSystem; } }

namespace rtype::server::game {

class GameSession {
  public:
    GameSession() = default;
    rt::ecs::Registry& registry() { return reg_; }

    // Initialize all ECS systems and expose spawn system pointer for tuning
    void initSystems(std::mt19937& rng, float* elapsedPtr,
                     std::uint8_t difficulty,
                     std::uint8_t shooterPercent,
                     rt::game::FormationSpawnSystem** outSpawnSys);

    void update(float dt);

  private:
    rt::ecs::Registry reg_;
};

}
