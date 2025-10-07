#pragma once
#include <cstdint>
#include <array>

namespace rtype::net {

enum class MsgType : std::uint8_t {
    Hello = 1,
    HelloAck,
    Input,
    State,
    Spawn,
    Despawn,
    Ping,
    Pong,
    // New messages
    Disconnect,     // client -> server: explicit disconnect notice
    ReturnToMenu    // server -> client: ask client to return to menu (e.g., too few players)
};

struct Header {
    std::uint16_t size;   // payload size excluding header
    MsgType type;
    std::uint8_t version;
};

static constexpr std::uint8_t ProtocolVersion = 1;
static constexpr std::size_t HeaderSize = sizeof(Header);

// --- Minimal binary protocol for inputs and world state ---

// Bitmask for client inputs
enum : std::uint8_t {
    InputUp    = 1 << 0,
    InputDown  = 1 << 1,
    InputLeft  = 1 << 2,
    InputRight = 1 << 3,
    InputShoot = 1 << 4,
};

// Simple entity types used for rendering
enum class EntityType : std::uint8_t {
    Player = 1,
    Enemy  = 2,
    Bullet = 3,
};

#pragma pack(push, 1)
struct InputPacket {
    std::uint32_t sequence; // client-side increasing sequence id
    std::uint8_t bits;      // combination of Input* bits
};

struct PackedEntity {
    std::uint32_t id;
    EntityType type;
    float x;
    float y;
    float vx;
    float vy;
    std::uint32_t rgba; // 0xRRGGBBAA
};

// The State payload is: StateHeader + N * PackedEntity
struct StateHeader {
    std::uint16_t count; // number of entities following
};
#pragma pack(pop)

}
