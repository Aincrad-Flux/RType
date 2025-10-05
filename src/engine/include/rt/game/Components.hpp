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

// Component carried by formation origin entity
struct Formation {
    FormationType type = FormationType::None;
    float speedX = -80.f;
    float amplitude = 60.f; // for snake
    float frequency = 2.0f; // for snake
    float spacing = 32.f;   // generic spacing
    int rows = 0;           // for grid/triangle
    int cols = 0;           // for grid
};

// Component for followers (enemies) attached to a formation origin entity
struct FormationFollower {
    rt::ecs::Entity formation = 0; // entity id of origin
    std::uint16_t index = 0;       // index in formation
    float localX = 0.f;            // local offset from origin
    float localY = 0.f;
};

}
