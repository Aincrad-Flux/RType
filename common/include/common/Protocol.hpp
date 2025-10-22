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
    Roster,     // list of players with names and lives (sent on join/leave)
    LivesUpdate, // notify when a player's lives change
    ScoreUpdate, // notify when a player's score changes (authoritative)
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
    InputCharge = 1 << 5, // hold to charge special shot
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

// --- NEW: unified per-frame client package ---
// Groups input + optional gameplay state in one packet
struct ClientPackage {
    std::uint32_t sequence;   // frame counter or timestamp
    std::uint8_t inputBits;   // bitmask of current pressed keys
    std::uint8_t actionFlags; // reserved for special actions (shoot, charge, etc.)
    float chargeLevel;        // current charge amount (0.0f if unused)
    std::uint32_t pingTime;   // optional timestamp for ping/pong tracking
    char username[16];        // zero-padded/truncated username (max 15 chars + NUL)
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

// --- Lightweight roster message (player list) ---
// Payload layout: RosterHeader + count * PlayerEntry
#pragma pack(push, 1)
struct RosterHeader {
    std::uint8_t count; // number of PlayerEntry records following
};

// Fixed-size per player entry to avoid dynamic parsing; name is UTF-8 truncated
struct PlayerEntry {
    std::uint32_t id;     // server-side entity/player id
    std::uint8_t lives;   // remaining lives
    char name[16];        // zero-padded/truncated username (max 15 chars + NUL)
};
#pragma pack(pop)

// One-off update for a single player's lives change
#pragma pack(push, 1)
struct LivesUpdatePayload {
    std::uint32_t id;
    std::uint8_t lives; // new lives value
};
#pragma pack(pop)

// One-off update for a single player's score change
#pragma pack(push, 1)
struct ScoreUpdatePayload {
    std::uint32_t id;
    std::int32_t score; // new total score
};
#pragma pack(pop)

}
