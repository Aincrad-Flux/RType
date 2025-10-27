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
  // Difficulty: 0=Easy,1=Normal,2=Hard
  void setDifficulty(std::uint8_t diff) {
    diff = std::min<std::uint8_t>(diff, 2);
    difficulty_ = diff;
    switch (difficulty_) {
      case 0: baseInterval_ = 3.5f; countMultiplier_ = 0.8f; break;
      case 1: baseInterval_ = 3.0f; countMultiplier_ = 1.0f; break;
      case 2: baseInterval_ = 2.0f; countMultiplier_ = 1.4f; break;
      default: baseInterval_ = 3.0f; countMultiplier_ = 1.0f; break;
    }
  }
  void setShooterPercent(std::uint8_t percent) { shooterPercent_ = percent; }
    void update(rt::ecs::Registry& r, float dt) override;
  private:
    std::mt19937& rng_;
    float timer_ = 0.f;
    float* t_;
  // difficulty knobs
  std::uint8_t difficulty_ = 1; // 0..2
  float baseInterval_ = 3.0f;
  float countMultiplier_ = 1.0f;
  std::uint8_t shooterPercent_ = 25; // 0..100
    // Internal helpers to create formations
    rt::ecs::Entity spawnSnake(rt::ecs::Registry& r, float y, int count);
    rt::ecs::Entity spawnLine(rt::ecs::Registry& r, float y, int count);
    rt::ecs::Entity spawnGrid(rt::ecs::Registry& r, float y, int rows, int cols);
    rt::ecs::Entity spawnTriangle(rt::ecs::Registry& r, float y, int rows);
    rt::ecs::Entity spawnBigShooters(rt::ecs::Registry& r, float y, int count);
};

class CollisionSystem : public rt::ecs::System {
  public:
    void update(rt::ecs::Registry& r, float dt) override;
};

}
