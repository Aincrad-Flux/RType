# Common Data Structures

## Overview

This document describes the common data structures and types used across multiple message types in the R-Type protocol. All structures use `#pragma pack(push, 1)` to ensure consistent binary layout without padding.

## Struct Packing

### Packing Directive

All payload structures are defined with:

```cpp
#pragma pack(push, 1)
struct PayloadStruct {
    // fields
};
#pragma pack(pop)
```

**Effect:** Removes all padding between struct members, ensuring the binary layout matches the field order exactly.

**Important:** Both sender and receiver must use the same packing directive.

### Example of Packing Impact

**Without Packing:**
```cpp
struct Example {
    uint8_t a;   // 1 byte
    // 3 bytes padding (for alignment)
    uint32_t b;  // 4 bytes
    uint8_t c;   // 1 byte
    // 3 bytes padding
};
// Total: 12 bytes
```

**With `#pragma pack(1)`:**
```cpp
#pragma pack(push, 1)
struct Example {
    uint8_t a;   // 1 byte
    uint32_t b;  // 4 bytes
    uint8_t c;   // 1 byte
};
#pragma pack(pop)
// Total: 6 bytes
```

## Enumeration Types

### MsgType

**Purpose:** Identifies the message type for routing

**Definition:**
```cpp
enum class MsgType : std::uint8_t {
    // UDP messages (1-99)
    Hello = 1,
    HelloAck = 2,
    Input = 3,
    State = 4,
    Spawn = 5,
    Despawn = 6,
    Roster = 9,
    LivesUpdate = 10,
    ScoreUpdate = 11,
    Disconnect = 12,
    ReturnToMenu = 13,

    // TCP messages (100+)
    TcpWelcome = 100,
    StartGame = 101
};
```

**Size:** 1 byte (stored as `uint8_t`)

**Source:** `common/include/common/Protocol.hpp`

### EntityType

**Purpose:** Identifies entity type for rendering

**Definition:**
```cpp
enum class EntityType : std::uint8_t {
    Player = 1,
    Enemy  = 2,
    Bullet = 3,
};
```

**Size:** 1 byte

**Usage:** In `PackedEntity` struct, tells client how to render the entity

**Rendering Implications:**
- `Player`: Render as player ship, apply player-specific logic
- `Enemy`: Render as enemy ship, animate according to enemy type
- `Bullet`: Render as projectile, simple sprite or particle

**Source:** `common/include/common/Protocol.hpp`

### Input Bitmask

**Purpose:** Compact representation of player input state

**Definition:**
```cpp
enum : std::uint8_t {
    InputUp     = 1 << 0,  // 0x01
    InputDown   = 1 << 1,  // 0x02
    InputLeft   = 1 << 2,  // 0x04
    InputRight  = 1 << 3,  // 0x08
    InputShoot  = 1 << 4,  // 0x10
    InputCharge = 1 << 5,  // 0x20
};
```

**Size:** 1 byte (8 bits, 2 bits currently unused)

**Usage:** Combine bits with bitwise OR:
```cpp
uint8_t input_bits = InputUp | InputRight | InputShoot;
// Result: 0x01 | 0x08 | 0x10 = 0x19
```

**Test bits:**
```cpp
if (input_bits & InputUp) {
    // Player is pressing up
}
```

**Implementation Status:**
- `InputUp`, `InputDown`, `InputLeft`, `InputRight`: **Active** (movement)
- `InputShoot`: **Reserved** (defined but not implemented in server)
- `InputCharge`: **Reserved** (defined but not implemented in server)

**Source:** `common/include/common/Protocol.hpp`

## Core Structures

### InputPacket

**Purpose:** Sent by client to report current input state

**Definition:**
```cpp
#pragma pack(push, 1)
struct InputPacket {
    std::uint32_t sequence;  // Client-side increasing sequence ID
    std::uint8_t bits;       // Combination of Input* bits
};
#pragma pack(pop)
```

**Size:** 5 bytes

**Binary Layout:**

| Offset | Size | Type | Field | Description |
|--------|------|------|-------|-------------|
| 0 | 4 bytes | `uint32_t` | `sequence` | Sequence number (little-endian) |
| 4 | 1 byte | `uint8_t` | `bits` | Input bitmask |

**Field Details:**

**sequence:**
- Client-generated, monotonically increasing per input sent
- **Currently unused by server** (server applies latest input regardless of sequence)
- **Future use:** Detect out-of-order or duplicate inputs
- Endianness: Little-endian
- Example: 0, 1, 2, 3, ...

**bits:**
- Bitmask of currently pressed inputs
- See "Input Bitmask" section above
- Example: `0x09` = Up (0x01) + Right (0x08)

**Example Wire Format:**

Input with sequence=42, Right+Shoot pressed:
```
Hex: 2A 00 00 00 18
     └─ sequence=42 (0x0000002A in little-endian)
                  └─ bits=0x18 (Right|Shoot = 0x08|0x10)
```

**Source:** `common/include/common/Protocol.hpp`

### PackedEntity

**Purpose:** Compact representation of a game entity in world state

**Definition:**
```cpp
#pragma pack(push, 1)
struct PackedEntity {
    std::uint32_t id;       // Unique entity identifier
    EntityType type;        // Entity type (uint8_t)
    float x;                // X position
    float y;                // Y position
    float vx;               // X velocity
    float vy;               // Y velocity
    std::uint32_t rgba;     // Color (0xRRGGBBAA)
};
#pragma pack(pop)
```

**Size:** 25 bytes

**Binary Layout:**

| Offset | Size | Type | Field | Description |
|--------|------|------|-------|-------------|
| 0 | 4 bytes | `uint32_t` | `id` | Entity ID (little-endian) |
| 4 | 1 byte | `EntityType` | `type` | Entity type (1=Player, 2=Enemy, 3=Bullet) |
| 5 | 4 bytes | `float` | `x` | X coordinate (IEEE-754, little-endian) |
| 9 | 4 bytes | `float` | `y` | Y coordinate (IEEE-754, little-endian) |
| 13 | 4 bytes | `float` | `vx` | X velocity (IEEE-754, little-endian) |
| 17 | 4 bytes | `float` | `vy` | Y velocity (IEEE-754, little-endian) |
| 21 | 4 bytes | `uint32_t` | `rgba` | Color (0xRRGGBBAA, little-endian) |

**Field Details:**

**id:**
- Unique identifier for this entity
- Stable across multiple State messages (same entity keeps same ID)
- Assigned by server, never reused within a game session
- Used by client to track entities frame-to-frame

**type:**
- See `EntityType` enum
- Determines how client renders and interprets the entity

**x, y:**
- Entity position in world coordinates
- Origin and scale are game-specific (typically pixels)
- Example: x=100.5, y=200.0

**vx, vy:**
- Entity velocity in world units per second
- Used for client-side interpolation/prediction
- Example: vx=-60.0 (moving left at 60 units/sec)

**rgba:**
- Color in RGBA format: `0xRRGGBBAA`
  - `RR`: Red (0x00-0xFF)
  - `GG`: Green (0x00-0xFF)
  - `BB`: Blue (0x00-0xFF)
  - `AA`: Alpha (0x00-0xFF, 0xFF=opaque)
- Example: `0xFF0000FF` = red, fully opaque
- Example: `0x55AAFFFF` = light blue, fully opaque

**Float Encoding:**

All floats use IEEE-754 single precision (32-bit) in little-endian byte order.

Example: `123.456f`
- IEEE-754 hex: `0x42F6E979`
- Wire bytes (little-endian): `79 E9 F6 42`

**Source:** `common/include/common/Protocol.hpp`

### StateHeader

**Purpose:** Prefix for State message payload, indicates entity count

**Definition:**
```cpp
#pragma pack(push, 1)
struct StateHeader {
    std::uint16_t count;  // Number of entities following
};
#pragma pack(pop)
```

**Size:** 2 bytes

**Binary Layout:**

| Offset | Size | Type | Field | Description |
|--------|------|------|-------|-------------|
| 0 | 2 bytes | `uint16_t` | `count` | Number of `PackedEntity` structs following |

**Field Details:**

**count:**
- Number of `PackedEntity` structures that follow this header
- Range: 0 to 65535 (though server limits to 512)
- Endianness: Little-endian
- Used to calculate payload size: `payload_size = 2 + (count × 25)`

**Example:**

State with 3 entities:
```
Hex: 03 00 [... 75 bytes of entity data ...]
     └─ count=3 (0x0003)
```

**Source:** `common/include/common/Protocol.hpp`

### RosterHeader

**Purpose:** Prefix for Roster message payload, indicates player count

**Definition:**
```cpp
#pragma pack(push, 1)
struct RosterHeader {
    std::uint8_t count;  // Number of PlayerEntry records following
};
#pragma pack(pop)
```

**Size:** 1 byte

**Binary Layout:**

| Offset | Size | Type | Field | Description |
|--------|------|------|-------|-------------|
| 0 | 1 byte | `uint8_t` | `count` | Number of `PlayerEntry` structs following |

**Field Details:**

**count:**
- Number of `PlayerEntry` structures that follow this header
- Range: 0 to 255
- Used to calculate payload size: `payload_size = 1 + (count × 21)`

**Source:** `common/include/common/Protocol.hpp`

### PlayerEntry

**Purpose:** Compact representation of a player in the roster

**Definition:**
```cpp
#pragma pack(push, 1)
struct PlayerEntry {
    std::uint32_t id;     // Server-side entity/player ID
    std::uint8_t lives;   // Remaining lives
    char name[16];        // Zero-padded/truncated username (max 15 chars + NUL)
};
#pragma pack(pop)
```

**Size:** 21 bytes

**Binary Layout:**

| Offset | Size | Type | Field | Description |
|--------|------|------|-------|-------------|
| 0 | 4 bytes | `uint32_t` | `id` | Player entity ID (little-endian) |
| 4 | 1 byte | `uint8_t` | `lives` | Lives remaining |
| 5 | 16 bytes | `char[16]` | `name` | Player name (null-terminated, UTF-8) |

**Field Details:**

**id:**
- Matches the entity ID in State messages
- Client uses this to associate roster info with entities

**lives:**
- Number of lives remaining for this player
- Range: 0-255 (typically 0-5 in practice)
- 0 = dead/spectating

**name:**
- Fixed 16-byte buffer
- Null-terminated UTF-8 string
- Maximum 15 displayable characters + null terminator
- If name > 15 chars, truncated and null-terminated at byte 15
- Unused bytes should be zero-filled

**Example:**

Player ID=123, lives=3, name="Alice":
```
Hex: 7B 00 00 00 03 41 6C 69 63 65 00 00 00 00 00 00 00 00 00 00 00
     └─ id=123  └─ lives=3
                   └─ "Alice\0" (null-terminated, zero-padded to 16 bytes)
```

**Source:** `common/include/common/Protocol.hpp`

### LivesUpdatePayload

**Purpose:** Notify clients of a single player's lives change

**Definition:**
```cpp
#pragma pack(push, 1)
struct LivesUpdatePayload {
    std::uint32_t id;       // Player entity ID
    std::uint8_t lives;     // New lives value
};
#pragma pack(pop)
```

**Size:** 5 bytes

**Binary Layout:**

| Offset | Size | Type | Field | Description |
|--------|------|------|-------|-------------|
| 0 | 4 bytes | `uint32_t` | `id` | Player entity ID (little-endian) |
| 4 | 1 byte | `uint8_t` | `lives` | New lives count |

**Field Details:**

**id:**
- Entity ID of the player whose lives changed
- Client looks up this player in its roster

**lives:**
- New total lives for this player
- Not a delta; this is the absolute new value

**Source:** `common/include/common/Protocol.hpp`

### ScoreUpdatePayload

**Purpose:** Notify clients of a single player's score change

**Definition:**
```cpp
#pragma pack(push, 1)
struct ScoreUpdatePayload {
    std::uint32_t id;       // Player entity ID
    std::int32_t score;     // New total score
};
#pragma pack(pop)
```

**Size:** 8 bytes

**Binary Layout:**

| Offset | Size | Type | Field | Description |
|--------|------|------|-------|-------------|
| 0 | 4 bytes | `uint32_t` | `id` | Player entity ID (little-endian) |
| 4 | 4 bytes | `int32_t` | `score` | New score (signed, little-endian) |

**Field Details:**

**id:**
- Entity ID of the player whose score changed

**score:**
- New total score for this player
- Signed integer (can be negative if penalties exist)
- Not a delta; this is the absolute new value
- Range: -2,147,483,648 to 2,147,483,647

**Source:** `common/include/common/Protocol.hpp`

### HelloAckPayload

**Purpose:** Server's response to TCP Hello, provides UDP port and token

**Definition:**
```cpp
#pragma pack(push, 1)
struct HelloAckPayload {
    std::uint16_t udpPort;  // UDP port to use
    std::uint32_t token;    // Session token to present in UDP Hello
};
#pragma pack(pop)
```

**Size:** 6 bytes

**Binary Layout:**

| Offset | Size | Type | Field | Description |
|--------|------|------|-------|-------------|
| 0 | 2 bytes | `uint16_t` | `udpPort` | UDP port number (little-endian) |
| 2 | 4 bytes | `uint32_t` | `token` | Authentication token (little-endian) |

**Field Details:**

**udpPort:**
- UDP port number where server is listening for gameplay traffic
- Client must send UDP Hello to this port
- Typically same as TCP port (4242) but can be different

**token:**
- Authentication token generated by server
- Client must include this in UDP Hello message
- Server validates token before accepting UDP session
- Token format: arbitrary uint32, server-defined (could be random, sequential, etc.)

**Security Note:** Token is sent in plaintext over TCP. If TCP connection is not encrypted, token can be intercepted.

**Source:** `common/include/common/Protocol.hpp`

### UdpHelloPayload

**Purpose:** Client's UDP Hello with authentication token

**Definition:**
```cpp
#pragma pack(push, 1)
struct UdpHelloPayload {
    std::uint32_t token;  // Must match token from TCP HelloAck
    char name[16];        // Optional username (0-terminated/truncated)
};
#pragma pack(pop)
```

**Size:** 20 bytes

**Binary Layout:**

| Offset | Size | Type | Field | Description |
|--------|------|------|-------|-------------|
| 0 | 4 bytes | `uint32_t` | `token` | Authentication token (little-endian) |
| 4 | 16 bytes | `char[16]` | `name` | Player name (null-terminated, UTF-8) |

**Field Details:**

**token:**
- Token received from TCP `HelloAck` message
- Server validates this against expected token
- If invalid, server ignores the Hello

**name:**
- Player's chosen display name
- Same format as `PlayerEntry.name`
- Null-terminated UTF-8, max 15 chars + null
- Optional: can be empty string

**Source:** `common/include/common/Protocol.hpp`

## Endianness and Portability

### Current Implementation

**All multi-byte values use little-endian (native) format:**
- `uint16_t`: Low byte first
- `uint32_t`: Low byte first
- `int32_t`: Low byte first
- `float`: IEEE-754 little-endian

**No Byte Swapping:**
- No calls to `htons()`, `htonl()`, `ntohl()`, `ntohs()`
- Sender and receiver must have same endianness

### Portability Implications

**Works On:**
- x86 (Intel/AMD) - little-endian
- x86-64 (Intel/AMD) - little-endian
- ARM (most configurations) - little-endian
- RISC-V (most configurations) - little-endian

**Does NOT Work On:**
- PowerPC (big-endian mode)
- SPARC (big-endian)
- MIPS (big-endian mode)
- ARM (big-endian mode, rare)

### Making It Portable (Future)

To support mixed-endian environments:

1. **Define network byte order** (typically big-endian)
2. **Add conversion functions:**
   ```cpp
   uint16_t to_network(uint16_t host) { return htons(host); }
   uint16_t from_network(uint16_t net) { return ntohs(net); }
   // Similar for uint32_t, int32_t
   ```
3. **For floats:** Use memcpy to uint32_t, convert, memcpy back
4. **Update all serialization code**

## Size Reference Table

| Structure | Size (bytes) | Usage |
|-----------|--------------|-------|
| `Header` | 4 | Every message |
| `InputPacket` | 5 | Input payload |
| `PackedEntity` | 25 | Per entity in State |
| `StateHeader` | 2 | State payload prefix |
| `RosterHeader` | 1 | Roster payload prefix |
| `PlayerEntry` | 21 | Per player in Roster |
| `LivesUpdatePayload` | 5 | LivesUpdate payload |
| `ScoreUpdatePayload` | 8 | ScoreUpdate payload |
| `HelloAckPayload` | 6 | TCP HelloAck payload |
| `UdpHelloPayload` | 20 | UDP Hello payload |

## Validation Checklist

When implementing a client or server, validate:

### Size Validation
- [ ] `received_bytes >= HeaderSize` (4)
- [ ] `header.size == (received_bytes - HeaderSize)`
- [ ] For `Input`: `payload_size >= sizeof(InputPacket)` (5)
- [ ] For `State`: `payload_size >= 2 + (count × 25)`
- [ ] For `Roster`: `payload_size >= 1 + (count × 21)`

### Type Validation
- [ ] `header.version == 1`
- [ ] `header.type` is known value (or ignore if unknown)
- [ ] `EntityType` is 1, 2, or 3 (or handle unknown gracefully)

### String Validation
- [ ] Player names are null-terminated within 16 bytes
- [ ] Names are valid UTF-8 (or handle invalid gracefully)

## Related Documentation

- **[01-header.md](01-header.md)** - Message header
- **[udp-03-input.md](udp-03-input.md)** - Input message usage
- **[udp-04-state.md](udp-04-state.md)** - State message usage
- **[udp-09-roster.md](udp-09-roster.md)** - Roster message usage

---

**Last Updated:** 2025-10-29

