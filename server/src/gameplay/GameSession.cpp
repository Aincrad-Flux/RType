#include "gameplay/GameSession.hpp"
#include "rt/game/Components.hpp"
#include "rt/game/Systems.hpp"

using namespace rtype::server::game;

void GameSession::initSystems(std::mt19937& rng, float* elapsedPtr,
                              std::uint8_t difficulty,
                              std::uint8_t shooterPercent,
                              rt::game::FormationSpawnSystem** outSpawnSys) {
    reg_.addSystem(std::make_unique<rt::game::InputSystem>());
    reg_.addSystem(std::make_unique<rt::game::ShootingSystem>());
    reg_.addSystem(std::make_unique<rt::game::ChargeShootingSystem>());
    reg_.addSystem(std::make_unique<rt::game::FormationSystem>(elapsedPtr));
    reg_.addSystem(std::make_unique<rt::game::MovementSystem>());
    reg_.addSystem(std::make_unique<rt::game::EnemyShootingSystem>(rng));
    reg_.addSystem(std::make_unique<rt::game::DespawnOffscreenSystem>(-50.f));
    reg_.addSystem(std::make_unique<rt::game::DespawnOutOfBoundsSystem>(-50.f, 1000.f, -50.f, 600.f));
    reg_.addSystem(std::make_unique<rt::game::CollisionSystem>());
    reg_.addSystem(std::make_unique<rt::game::InvincibilitySystem>());
    {
        auto sys = std::make_unique<rt::game::FormationSpawnSystem>(rng, elapsedPtr);
        auto* ptr = sys.get();
        ptr->setDifficulty(difficulty);
        ptr->setShooterPercent(shooterPercent);
        if (outSpawnSys) *outSpawnSys = ptr;
        reg_.addSystem(std::move(sys));
    }
}

void GameSession::update(float dt) { reg_.update(dt); }
