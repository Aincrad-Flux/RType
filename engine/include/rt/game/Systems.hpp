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

// Create bullets when player holds shoot; uses Shooter cooldown
class ShootingSystem : public rt::ecs::System {
  public:
    void update(rt::ecs::Registry& r, float dt) override;
};

// Processes charge input and spawns a beam entity based on charge duration
class ChargeShootingSystem : public rt::ecs::System {
  public:
    void update(rt::ecs::Registry& r, float dt) override;
};

// Enemy AI shooting towards players with limited accuracy
class EnemyShootingSystem : public rt::ecs::System {
  public:
    EnemyShootingSystem(std::mt19937& rng) : rng_(rng) {}
    void update(rt::ecs::Registry& r, float dt) override;
  private:
    std::mt19937& rng_;
};

class FormationSystem : public rt::ecs::System {
  public:
    FormationSystem(float* elapsedPtr) : t_(elapsedPtr) {}
    void update(rt::ecs::Registry& r, float dt) override;
  private:
    float* t_; // pointer to elapsed time provided by host
};

class DespawnOffscreenSystem : public rt::ecs::System {
  public:
    explicit DespawnOffscreenSystem(float minX) : minX_(minX) {}
    void update(rt::ecs::Registry& r, float dt) override;
  private:
    float minX_;
};

// Despawn when leaving screen bounds (for bullets etc.)
class DespawnOutOfBoundsSystem : public rt::ecs::System {
  public:
    DespawnOutOfBoundsSystem(float minX, float maxX, float minY, float maxY)
        : minX_(minX), maxX_(maxX), minY_(minY), maxY_(maxY) {}
    void update(rt::ecs::Registry& r, float dt) override;
  private:
    float minX_, maxX_, minY_, maxY_;
};

// Ticks invincibility timers on players and clears HitFlag markers
class InvincibilitySystem : public rt::ecs::System {
  public:
    void update(rt::ecs::Registry& r, float dt) override;
};

// Spawns enemy formations at intervals
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
    rt::ecs::Entity spawnBigShooters(rt::ecs::Registry& r, float y, int count);
};

// Handle simple AABB collisions: bullets vs enemies
class CollisionSystem : public rt::ecs::System {
  public:
    void update(rt::ecs::Registry& r, float dt) override;
};

}
