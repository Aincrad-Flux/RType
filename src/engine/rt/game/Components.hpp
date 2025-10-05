#pragma once
#include <cstdint>
#include "rt/ecs/Types.hpp"
#include "common/Protocol.hpp"

namespace rt::game {

struct Transform { float x = 0.f; float y = 0.f; };
struct Velocity { float vx = 0.f; float vy = 0.f; };
struct ColorRGBA { std::uint32_t rgba = 0xFFFFFFFFu; };
struct NetType { rtype::net::EntityType type = rtype::net::EntityType::Player; };

struct PlayerInput { std::uint8_t bits = 0; float speed = 150.f; };
struct EnemyTag {};

enum class FormationType : std::uint8_t { None = 0, Snake, Line, GridRect, Triangle };

struct Formation {
    FormationType type = FormationType::None;
    float speedX = -80.f;
    float amplitude = 60.f;
    float frequency = 2.0f;
    float spacing = 32.f;
    int rows = 0;
    int cols = 0;
};

struct FormationFollower {
    rt::ecs::Entity formation = 0;
    std::uint16_t index = 0;
    float localX = 0.f;
    float localY = 0.f;
};

}
