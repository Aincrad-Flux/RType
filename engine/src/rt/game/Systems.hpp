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

class ShootingSystem : public rt::ecs::System {
  public:
    void update(rt::ecs::Registry& r, float dt) override;
};

class ChargeShootingSystem : public rt::ecs::System {
  public:
    void update(rt::ecs::Registry& r, float dt) override;
};

class EnemyShootingSystem : public rt::ecs::System {
  public:
    explicit EnemyShootingSystem(std::mt19937& rng) : rng_(rng) {}
    void update(rt::ecs::Registry& r, float dt) override;
  private:
    std::mt19937& rng_;
};

class FormationSystem : public rt::ecs::System {
  public:
    explicit FormationSystem(float* elapsedPtr) : t_(elapsedPtr) {}
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

class DespawnOutOfBoundsSystem : public rt::ecs::System {
  public:
    DespawnOutOfBoundsSystem(float minX, float maxX, float minY, float maxY)
        : minX_(minX), maxX_(maxX), minY_(minY), maxY_(maxY) {}
    void update(rt::ecs::Registry& r, float dt) override;
  private:
    float minX_, maxX_, minY_, maxY_;
};

class InvincibilitySystem : public rt::ecs::System {
  public:
    void update(rt::ecs::Registry& r, float dt) override;
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
    // When a boss is active we suppress regular waves; resume instantly afterwards
    bool blockedByBoss_ = false;
};

class CollisionSystem : public rt::ecs::System {
  public:
    void update(rt::ecs::Registry& r, float dt) override;
};

// Spawns the boss every time any player's score crosses a multiple of `threshold_`; prevents other spawns while active
class BossSpawnSystem : public rt::ecs::System {
  public:
    explicit BossSpawnSystem(int threshold = 15000) : threshold_(threshold) {}
    void update(rt::ecs::Registry& r, float dt) override;
  private:
    int threshold_ = 15000;
    // Number of bosses spawned so far (for threshold multiples)
    int bossesSpawned_ = 0;
    // Track if a boss is currently alive to avoid double spawns in the same frame
    bool bossActive_ = false;
};

// Controls the boss movement (slide in from right, then bounce vertically) and clamps it in world
class BossSystem : public rt::ecs::System {
  public:
    void update(rt::ecs::Registry& r, float dt) override;
};

}
