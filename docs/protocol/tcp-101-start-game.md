# StartGame (101)

## Overview

**Message Type:** `StartGame` (101)
**Transport:** TCP
**Direction:** Server → Client
**Purpose:** Signals that the game is ready to start, sent after TcpWelcome
**Status:** Active

**Note:** Based on the code structure, this message likely follows `TcpWelcome`. The exact payload format should be verified in the server implementation.

## Message Format

### Expected Structure

```
┌─────────────────────────┬───────────────────────┐
│   Header (4 bytes)      │  Payload (variable)   │
├─────────────────────────┼───────────────────────┤
│ size=?, type=101, ver=1 │  StartGame payload    │
└─────────────────────────┴───────────────────────┘
```

### Header

| Field | Value | Encoding |
|-------|-------|----------|
| `size` | Varies | `uint16_t` (little-endian) |
| `type` | 101 | `uint8_t` (StartGame) |
| `version` | 1 | `uint8_t` |

**Wire Format (Header):**
```
XX XX 65 01
└─ size (varies)
      └─ type=101 (0x65)
         └─ version=1
```

## Payload Format

### Possible Implementations

The `StartGame` message type is defined in the protocol but the exact payload structure should be determined by the server implementation. Common patterns include:

**Pattern 1: Empty Payload (Signal Only)**
```cpp
// No payload structure needed
// size = 0
```

**Pattern 2: Game Configuration**
```cpp
#pragma pack(push, 1)
struct StartGamePayload {
    uint32_t game_id;        // Unique game session ID
    uint8_t max_players;     // Maximum players allowed
    uint8_t current_players; // Currently connected players
    uint16_t map_id;         // Map/level identifier
};
#pragma pack(pop)
// size = 8
```

**Pattern 3: Extended with Room Info**
```cpp
#pragma pack(push, 1)
struct StartGamePayload {
    uint32_t game_id;
    uint32_t seed;           // Random seed for procedural generation
    uint8_t difficulty;      // Difficulty level
    char room_name[32];      // Room name
};
#pragma pack(pop)
// size = 41
```

## Protocol Flow

### Connection Sequence

```
Client                                Server
   │                                     │
   │───── TCP Connect ────────────────────>│
   │                                     │
   │<──── TcpWelcome (port, token) ───────│
   │                                     │
   │<──── StartGame (game info) ──────────│
   │                                     │
   │ [Client ready to start]             │
   │ [Switch to UDP]                     │
   │                                     │
   │───── UDP Hello (token) ──────────────>│
   │                                     │
```

**Timing:**
- Sent immediately after `TcpWelcome` in current implementation
- May be delayed if server waits for lobby to fill (future feature)

## Field Specifications

**Note:** The following are suggested fields based on common game networking patterns. Actual implementation should be verified.

### game_id (if present)

**Type:** `uint32_t` (4 bytes, little-endian)

**Purpose:** Unique identifier for this game session

**Usage:**
- Client can use for logging and debugging
- May be used for reconnection attempts
- Server can use for session management

### max_players (if present)

**Type:** `uint8_t` (1 byte)

**Purpose:** Maximum number of players in this game

**Range:** 1-255 (typical: 2-8)

### current_players (if present)

**Type:** `uint8_t` (1 byte)

**Purpose:** Number of players already connected when game starts

**Usage:**
- Client can display "X/Y players"
- May be used for initial UI setup

## Implementation Examples

### Server (Sending) - Empty Payload

```cpp
#include "common/Protocol.hpp"
#include <asio.hpp>
#include <cstring>

using namespace rtype::net;

void sendStartGame(asio::ip::tcp::socket& socket) {
    // Prepare header (no payload)
    Header header;
    header.size = 0;  // No payload
    header.type = MsgType::StartGame;
    header.version = ProtocolVersion;

    // Send header only
    asio::write(socket, asio::buffer(&header, sizeof(Header)));
}
```

### Server (Sending) - With Payload

```cpp
struct StartGamePayload {
    uint32_t game_id;
    uint8_t max_players;
    uint8_t current_players;
};

void sendStartGame(asio::ip::tcp::socket& socket, uint32_t game_id, uint8_t max, uint8_t current) {
    Header header;
    header.size = sizeof(StartGamePayload);
    header.type = MsgType::StartGame;
    header.version = ProtocolVersion;

    StartGamePayload payload{game_id, max, current};

    // Combine and send
    uint8_t buffer[sizeof(Header) + sizeof(StartGamePayload)];
    std::memcpy(buffer, &header, sizeof(Header));
    std::memcpy(buffer + sizeof(Header), &payload, sizeof(StartGamePayload));

    asio::write(socket, asio::buffer(buffer, sizeof(buffer)));
}
```

### Client (Receiving)

```cpp
#include "common/Protocol.hpp"
#include <asio.hpp>
#include <vector>

using namespace rtype::net;

bool receiveStartGame(asio::ip::tcp::socket& socket) {
    // Read header
    Header header;
    asio::read(socket, asio::buffer(&header, sizeof(Header)));

    // Validate
    if (header.version != ProtocolVersion) {
        return false;
    }

    if (header.type != MsgType::StartGame) {
        return false;
    }

    // Read payload (if any)
    if (header.size > 0) {
        std::vector<uint8_t> payload(header.size);
        asio::read(socket, asio::buffer(payload));

        // Parse payload based on expected structure
        // ...
    }

    // Game is ready to start
    return true;
}
```

## Client Behavior

### On Receiving StartGame

1. **Validate Message:**
   - Check version, type, size

2. **Parse Payload:**
   - Extract game configuration (if present)

3. **Prepare for Gameplay:**
   - Initialize game state
   - Load assets if needed
   - Prepare UI

4. **Switch to UDP:**
   - Close TCP connection (or keep for lobby/chat)
   - Open UDP socket
   - Send UDP Hello with token

5. **Start Game Loop:**
   - Begin sending Input messages
   - Begin receiving State messages

## Validation

### Client Validation

**Required:**
- [ ] `header.version == 1`
- [ ] `header.type == 101` (StartGame)
- [ ] If `header.size > 0`, validate payload structure

**Error Handling:**
```cpp
if (header.version != ProtocolVersion) {
    // Protocol version mismatch
    disconnect();
    showError("Incompatible server version");
    return;
}

if (header.type != MsgType::StartGame) {
    // Unexpected message
    // May be out of order or protocol error
    return;
}
```

## Wire Format Examples

### Example 1: Empty Payload (Signal Only)

**Complete Message:**
```
Offset  Hex                      ASCII       Description
------  ----------------------   -------     -----------
0x0000  00 00 65 01              ..e.        Header (size=0, type=101, version=1)
```

**Breakdown:**
- `00 00`: size = 0 (no payload)
- `65`: type = 101 (0x65 = StartGame)
- `01`: version = 1

**Total Size:** 4 bytes

### Example 2: With Game Configuration

**Payload Structure (assumed):**
```cpp
struct StartGamePayload {
    uint32_t game_id;        // e.g., 0x00001234
    uint8_t max_players;     // e.g., 4
    uint8_t current_players; // e.g., 2
};
```

**Complete Message:**
```
Offset  Hex                      ASCII       Description
------  ----------------------   -------     -----------
0x0000  06 00 65 01              ..e.        Header (size=6, type=101, version=1)
0x0004  34 12 00 00              4...        game_id=0x1234
0x0008  04                       .           max_players=4
0x0009  02                       .           current_players=2
```

**Total Size:** 10 bytes

## Common Issues

### Issue 1: Client Doesn't Receive StartGame

**Problem:** Client receives TcpWelcome but not StartGame

**Possible Causes:**
1. Server implementation doesn't send StartGame
2. TCP connection closed prematurely
3. Client reading logic has bug

**Debug:**
```bash
# Capture TCP traffic
sudo tcpdump -i any tcp port 4242 -X -s 0

# Check for second message after TcpWelcome
# Should see additional bytes with type=0x65
```

### Issue 2: Parse Error

**Problem:** Client can't parse StartGame payload

**Causes:**
1. Client expects payload but server sends empty (or vice versa)
2. Struct packing mismatch
3. Endianness issue

**Solution:**
- Log `header.size` on both sides
- Verify struct definitions match
- Check `#pragma pack` usage

## Future Considerations

### Lobby System

If implementing a lobby system, `StartGame` could include:

```cpp
#pragma pack(push, 1)
struct StartGamePayload {
    uint32_t game_id;
    uint32_t timestamp;      // Game start time
    uint8_t player_count;
    uint8_t ready_flags;     // Bitmask of ready players
    uint16_t countdown_ms;   // Countdown before game starts
    char map_name[32];       // Map/level name
};
#pragma pack(pop)
```

### Matchmaking

For matchmaking features:

```cpp
#pragma pack(push, 1)
struct StartGamePayload {
    uint32_t match_id;
    uint8_t skill_bracket;   // Player skill level
    uint8_t team_assignment; // Which team this player is on
    uint16_t ranking_points; // Starting ranking points
};
#pragma pack(pop)
```

## Security Considerations

### Current State

**No additional security concerns** beyond those of `TcpWelcome`:
- Sent over plaintext TCP
- No authentication of game parameters
- Client must trust all values from server

### Potential Attacks

**Parameter Manipulation:**
- If TCP is not encrypted, attacker can modify game parameters
- Example: Change max_players to overflow client buffers

**Mitigation:**
- Use TLS for TCP
- Validate all parameters on client side
- Use reasonable limits (e.g., max_players ≤ 255)

## Testing

### Unit Test Example

```cpp
TEST(Protocol, StartGame_Empty) {
    Header header{0, MsgType::StartGame, ProtocolVersion};

    uint8_t buffer[4];
    std::memcpy(buffer, &header, 4);

    EXPECT_EQ(buffer[0], 0x00);  // size low byte
    EXPECT_EQ(buffer[1], 0x00);  // size high byte
    EXPECT_EQ(buffer[2], 101);   // type
    EXPECT_EQ(buffer[3], 1);     // version
}

TEST(Protocol, StartGame_WithPayload) {
    struct Payload {
        uint32_t game_id;
        uint8_t max_players;
    };

    Header header{sizeof(Payload), MsgType::StartGame, ProtocolVersion};
    Payload payload{0x1234, 4};

    uint8_t buffer[4 + sizeof(Payload)];
    std::memcpy(buffer, &header, 4);
    std::memcpy(buffer + 4, &payload, sizeof(Payload));

    EXPECT_EQ(buffer[2], 101);         // type
    EXPECT_EQ(buffer[4], 0x34);        // game_id low byte
    EXPECT_EQ(buffer[8], 4);           // max_players
}
```

### Integration Test

```cpp
TEST(Protocol, TCP_Handshake) {
    // Setup test server and client
    // ...

    // Client connects
    client.connect(server_endpoint);

    // Expect TcpWelcome
    auto welcome = client.receive_tcp_welcome();
    ASSERT_TRUE(welcome.has_value());

    // Expect StartGame
    auto start = client.receive_start_game();
    ASSERT_TRUE(start);

    // Client should now be ready for UDP
}
```

## Documentation Status

⚠️ **Note:** This documentation is based on the protocol definition in `Protocol.hpp`. The actual payload structure and server behavior should be verified by examining the server implementation code.

**To Complete This Documentation:**
1. Check server TCP handling code (likely in `server/TcpServer.cpp` or similar)
2. Identify the exact payload structure used (if any)
3. Update this document with confirmed implementation details

## Related Documentation

- **[tcp-100-welcome.md](tcp-100-welcome.md)** - Previous TCP message (TcpWelcome)
- **[udp-01-hello.md](udp-01-hello.md)** - Next message in connection flow (UDP Hello)
- **[02-transport.md](02-transport.md)** - TCP/UDP transport layer
- **[00-overview.md](00-overview.md)** - Protocol overview and connection flow

---

**Last Updated:** 2025-10-29
**Status:** ⚠️ Payload structure should be verified against implementation

