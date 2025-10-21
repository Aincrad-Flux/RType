#pragma once
#include <cstdint>
#include "rt/ecs/Types.hpp"
#include "common/Protocol.hpp" // For rtype::net::EntityType

namespace rt::game {

// Basic transform and kinematics
struct Transform { float x = 0.f; float y = 0.f; };
struct Velocity { float vx = 0.f; float vy = 0.f; };

// Visual / net metadata (kept for server serialization compatibility)
struct ColorRGBA { std::uint32_t rgba = 0xFFFFFFFFu; };
struct NetType { rtype::net::EntityType type; };

// Generic AABB size
struct Size { float w = 0.f; float h = 0.f; };

// Input component (server sets bits from network)
struct PlayerInput { std::uint8_t bits = 0; float speed = 150.f; };

// Tags and gameplay data used by server
struct EnemyTag {};

enum class BulletFaction : std::uint8_t { Player = 0, Enemy = 1 };
struct BulletTag { BulletFaction faction = BulletFaction::Player; };
struct BulletOwner { rt::ecs::Entity owner = 0; };

struct Shooter {
    float cooldown = 0.f;
    float interval = 0.15f;
    float bulletSpeed = 320.f;
};

struct BeamTag {};

struct ChargeGun { float charge = 0.f; float maxCharge = 2.0f; bool firing = false; };

struct EnemyShooter { float cooldown = 0.f; float interval = 1.0f; float bulletSpeed = 220.f; float accuracy = 0.6f; };

struct HitFlag { bool value = false; };
struct Invincible { float timeLeft = 0.f; };

struct Score { std::int32_t value = 0; };

enum class FormationType : std::uint8_t { None = 0, Snake, Line, GridRect, Triangle };
struct Formation { FormationType type = FormationType::None; float speedX = -60.f; float amplitude = 60.f; float frequency = 2.0f; float spacing = 32.f; int rows = 0; int cols = 0; };
struct FormationFollower { rt::ecs::Entity formation = 0; std::uint16_t index = 0; float localX = 0.f; float localY = 0.f; };

}
