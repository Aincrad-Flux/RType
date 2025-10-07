#pragma once
#include <random>
#include "rt/ecs/System.hpp"
#include "rt/ecs/Registry.hpp"
#include "rt/game/Components.hpp"

namespace rt::game {

class InputSystem : public rt::ecs::System {
  public:
    void update(rt::ecs::Registry& r, float dt) override;
};

class MovementSystem : public rt::ecs::System {
  public:
    void update(rt::ecs::Registry& r, float dt) override;
};

class FormationSystem : public rt::ecs::System {
  public:
    FormationSystem(float* elapsedPtr) : t_(elapsedPtr) {}
    void update(rt::ecs::Registry& r, float dt) override;
  private:
    float* t_;
};

class DespawnOffscreenSystem : public rt::ecs::System {
  public:
    explicit DespawnOffscreenSystem(float minX) : minX_(minX) {}
    void update(rt::ecs::Registry& r, float dt) override;
  private:
    float minX_;
};

class FormationSpawnSystem : public rt::ecs::System {
  public:
    FormationSpawnSystem(std::mt19937& rng, float* elapsedPtr)
        : rng_(rng), t_(elapsedPtr) {}
    void update(rt::ecs::Registry& r, float dt) override;
  private:
    std::mt19937& rng_;
    float timer_ = 0.f;
    float* t_;
    rt::ecs::Entity spawnSnake(rt::ecs::Registry& r, float y, int count);
    rt::ecs::Entity spawnLine(rt::ecs::Registry& r, float y, int count);
    rt::ecs::Entity spawnGrid(rt::ecs::Registry& r, float y, int rows, int cols);
    rt::ecs::Entity spawnTriangle(rt::ecs::Registry& r, float y, int rows);
};

}
