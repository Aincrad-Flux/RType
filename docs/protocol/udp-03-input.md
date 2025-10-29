# Input (3) - UDP

## Overview

**Message Type:** `Input` (3)
**Transport:** UDP
**Direction:** Client → Server
**Purpose:** Send player input (keyboard/gamepad state) to server
**Status:** Active
**Frequency:** 30-60 Hz recommended (or on input change)

## Message Format

### Complete Message Structure

```
┌─────────────────────────┬───────────────────────┐
│   Header (4 bytes)      │  Payload (5 bytes)    │
├─────────────────────────┼───────────────────────┤
│ size=5, type=3, ver=1   │  InputPacket          │
└─────────────────────────┴───────────────────────┘
```

**Total Message Size:** 9 bytes

### Header

| Field | Value | Encoding |
|-------|-------|----------|
| `size` | 5 | `uint16_t` (little-endian) |
| `type` | 3 | `uint8_t` (Input) |
| `version` | 1 | `uint8_t` |

**Wire Format (Header):**
```
05 00 03 01
└─ size=5
      └─ type=3 (Input)
         └─ version=1
```

### Payload: InputPacket

**Structure:**
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

## Field Specifications

### sequence

**Type:** `uint32_t` (4 bytes, little-endian)

**Purpose:** Client-side monotonically increasing identifier for each input sent

**Usage:**
- Client increments for each Input message sent (0, 1, 2, ...)
- **Currently unused by server** (server applies latest input regardless)
- **Future use:** Detect out-of-order delivery, reject old inputs, handle packet loss

**Example:**
```cpp
uint32_t input_sequence_ = 0;

void sendInput(uint8_t bits) {
    InputPacket packet;
    packet.sequence = input_sequence_++;
    packet.bits = bits;
    sendPacket(MsgType::Input, &packet, sizeof(packet));
}
```

**Wire Format:**

Sequence = 42 (0x0000002A):
```
2A 00 00 00
└─ 0x0000002A in little-endian
```

### bits

**Type:** `uint8_t` (1 byte)

**Purpose:** Bitmask representing currently pressed inputs

**Input Bit Definitions:**
```cpp
enum : std::uint8_t {
    InputUp     = 1 << 0,  // 0x01 - Move up
    InputDown   = 1 << 1,  // 0x02 - Move down
    InputLeft   = 1 << 2,  // 0x04 - Move left
    InputRight  = 1 << 3,  // 0x08 - Move right
    InputShoot  = 1 << 4,  // 0x10 - Fire weapon
    InputCharge = 1 << 5,  // 0x20 - Charge special shot
    // Bits 6-7 unused (0x40, 0x80)
};
```

**Implementation Status:**
- **Active:** `InputUp`, `InputDown`, `InputLeft`, `InputRight` (movement)
- **Reserved:** `InputShoot`, `InputCharge` (defined but server doesn't process)

**Combining Inputs:**
```cpp
// Example: Press Up and Right simultaneously
uint8_t bits = InputUp | InputRight;  // 0x01 | 0x08 = 0x09

// Example: Press Left and Shoot
uint8_t bits = InputLeft | InputShoot;  // 0x04 | 0x10 = 0x14

// Example: No input (all released)
uint8_t bits = 0;  // 0x00
```

**Testing Inputs:**
```cpp
// Check if Up is pressed
if (bits & InputUp) {
    // Move up
}

// Check if both Left and Shoot are pressed
if ((bits & (InputLeft | InputShoot)) == (InputLeft | InputShoot)) {
    // Moving left while shooting
}
```

**Wire Format:**

Moving right and shooting (InputRight | InputShoot = 0x08 | 0x10 = 0x18):
```
18
└─ 0x18 = 0b00011000
```

## Protocol Flow

### Input-State Loop

```
Client                                Server
   │                                     │
   │ [Player presses Up + Right]         │
   │                                     │
   │───── Input (seq=0, bits=0x09) ──────>│
   │                                     │  [Update player input state]
   │                                     │  [Tick: apply velocity]
   │                                     │  [Tick: update position]
   │                                     │
   │<──── State (all entities) ────────────│
   │                                     │
   │ [Player releases Up, still Right]   │
   │                                     │
   │───── Input (seq=1, bits=0x08) ──────>│
   │                                     │  [Update player input state]
   │                                     │
   │<──── State (all entities) ────────────│
   │                                     │
```

**Frequency:**
- Client sends Input at 30-60 Hz (or on input change)
- Server receives and stores latest input per player
- Server applies input at 60 Hz tick rate
- Server broadcasts State at 60 Hz

## Server Behavior

### Processing Input

**Current Implementation (from protocol docs):**

1. **Receive Input Message:**
   - Validate header (size >= 5, version == 1, type == Input)
   - Extract `bits` from payload (sequence currently ignored)

2. **Store Input State:**
   ```cpp
   std::unordered_map<uint32_t, uint8_t> player_input_bits_;

   void handleInput(uint32_t player_id, const InputPacket& input) {
       player_input_bits_[player_id] = input.bits;
   }
   ```

3. **Apply Input (Every Tick):**
   ```cpp
   void tick(float dt) {
       for (auto& [id, entity] : entities_) {
           if (entity.type != EntityType::Player) continue;

           uint8_t bits = player_input_bits_[id];
           float vx = 0.0f, vy = 0.0f;
           const float speed = 150.0f;  // px/s

           if (bits & InputUp)    vy -= speed;
           if (bits & InputDown)  vy += speed;
           if (bits & InputLeft)  vx -= speed;
           if (bits & InputRight) vx += speed;

           entity.vx = vx;
           entity.vy = vy;
           entity.x += vx * dt;
           entity.y += vy * dt;
       }
   }
   ```

**Properties:**
- **Latest State Wins:** Server only stores the most recent input per player
- **No Input Buffering:** Previous inputs are discarded
- **No Sequence Checking:** Server doesn't validate sequence numbers
- **Fixed Speed:** 150 px/s, dt = 1/60s (~16.67ms)

### Input Application

**Movement Calculation:**
- Single direction: velocity = ±150 px/s
- Diagonal (e.g., Up + Right): velocity = (±150, ±150) px/s
  - **No normalization:** diagonal speed is √2 × single direction (~212 px/s)

**Example Velocities:**
| Input | vx | vy | Actual Speed |
|-------|----|----|--------------|
| Right | +150 | 0 | 150 px/s |
| Up | 0 | -150 | 150 px/s |
| Up + Right | +150 | -150 | 212 px/s |
| Down + Left | -150 | +150 | 212 px/s |

## Client Behavior

### Sending Input

**Strategy 1: Fixed Rate (60 Hz)**
```cpp
const float INPUT_RATE = 60.0f;
const float INPUT_INTERVAL = 1.0f / INPUT_RATE;

float time_since_last_input_ = 0.0f;

void update(float dt) {
    time_since_last_input_ += dt;

    if (time_since_last_input_ >= INPUT_INTERVAL) {
        sendInput(getCurrentInputBits());
        time_since_last_input_ -= INPUT_INTERVAL;
    }
}
```

**Strategy 2: On Change**
```cpp
uint8_t last_sent_bits_ = 0;

void update() {
    uint8_t current_bits = getCurrentInputBits();

    if (current_bits != last_sent_bits_) {
        sendInput(current_bits);
        last_sent_bits_ = current_bits;
    }
}
```

**Strategy 3: Hybrid (Recommended)**
```cpp
uint8_t last_sent_bits_ = 0;
float time_since_last_input_ = 0.0f;
const float MAX_INPUT_INTERVAL = 1.0f / 30.0f;  // Max 30 Hz

void update(float dt) {
    uint8_t current_bits = getCurrentInputBits();
    time_since_last_input_ += dt;

    // Send if changed OR max interval reached (keep-alive)
    if (current_bits != last_sent_bits_ ||
        time_since_last_input_ >= MAX_INPUT_INTERVAL) {
        sendInput(current_bits);
        last_sent_bits_ = current_bits;
        time_since_last_input_ = 0.0f;
    }
}
```

### Capturing Input

**Keyboard Example:**
```cpp
uint8_t getCurrentInputBits() {
    uint8_t bits = 0;

    if (IsKeyDown(KEY_W) || IsKeyDown(KEY_UP))    bits |= InputUp;
    if (IsKeyDown(KEY_S) || IsKeyDown(KEY_DOWN))  bits |= InputDown;
    if (IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT))  bits |= InputLeft;
    if (IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT)) bits |= InputRight;
    if (IsKeyDown(KEY_SPACE))                     bits |= InputShoot;
    if (IsKeyDown(KEY_LEFT_SHIFT))                bits |= InputCharge;

    return bits;
}
```

**Gamepad Example:**
```cpp
uint8_t getCurrentInputBits() {
    uint8_t bits = 0;

    float axis_x = GetGamepadAxisMovement(0, GAMEPAD_AXIS_LEFT_X);
    float axis_y = GetGamepadAxisMovement(0, GAMEPAD_AXIS_LEFT_Y);

    const float threshold = 0.2f;

    if (axis_y < -threshold) bits |= InputUp;
    if (axis_y > threshold)  bits |= InputDown;
    if (axis_x < -threshold) bits |= InputLeft;
    if (axis_x > threshold)  bits |= InputRight;

    if (IsGamepadButtonDown(0, GAMEPAD_BUTTON_RIGHT_FACE_DOWN))
        bits |= InputShoot;

    return bits;
}
```

## Implementation Examples

### Client (Sending)

```cpp
#include "common/Protocol.hpp"
#include <asio.hpp>
#include <cstring>

using namespace rtype::net;

class GameClient {
    uint32_t input_sequence_ = 0;
    asio::ip::udp::socket& socket_;
    asio::ip::udp::endpoint server_endpoint_;

public:
    void sendInput(uint8_t input_bits) {
        // Prepare header
        Header header;
        header.size = sizeof(InputPacket);
        header.type = MsgType::Input;
        header.version = ProtocolVersion;

        // Prepare payload
        InputPacket payload;
        payload.sequence = input_sequence_++;
        payload.bits = input_bits;

        // Combine
        uint8_t buffer[sizeof(Header) + sizeof(InputPacket)];
        std::memcpy(buffer, &header, sizeof(Header));
        std::memcpy(buffer + sizeof(Header), &payload, sizeof(InputPacket));

        // Send
        socket_.send_to(asio::buffer(buffer, sizeof(buffer)), server_endpoint_);
    }
};
```

### Server (Receiving)

```cpp
#include "common/Protocol.hpp"
#include <unordered_map>
#include <cstring>

using namespace rtype::net;

class GameServer {
    std::unordered_map<uint32_t, uint8_t> player_input_bits_;

public:
    void handleInput(uint32_t player_id, const uint8_t* payload, size_t size) {
        // Validate size
        if (size < sizeof(InputPacket)) {
            return;  // Invalid
        }

        // Parse payload
        InputPacket input;
        std::memcpy(&input, payload, sizeof(InputPacket));

        // Store latest input for this player
        player_input_bits_[player_id] = input.bits;

        // Note: sequence is currently ignored
    }

    uint8_t getPlayerInput(uint32_t player_id) const {
        auto it = player_input_bits_.find(player_id);
        return (it != player_input_bits_.end()) ? it->second : 0;
    }
};
```

## Validation

### Client Validation (Before Sending)

**Optional Checks:**
- [ ] Input bits are valid (no undefined bits set)
- [ ] Sequence number hasn't overflowed (wraps at 2^32-1)

**Example:**
```cpp
void sendInput(uint8_t bits) {
    // Mask out unused bits (optional)
    bits &= 0x3F;  // Keep only bits 0-5

    // Send
    InputPacket packet{input_sequence_++, bits};
    send(packet);
}
```

### Server Validation (After Receiving)

**Required:**
- [ ] `header.version == 1`
- [ ] `header.type == 3` (Input)
- [ ] `header.size >= 5`
- [ ] `payload_size >= sizeof(InputPacket)`

**Optional:**
- [ ] Sequence number is greater than last (detect out-of-order)
- [ ] Sequence number delta is reasonable (detect replays)
- [ ] Input bits have no undefined bits set

**Example:**
```cpp
void validateAndHandleInput(uint32_t player_id, const Header& header,
                            const uint8_t* payload, size_t payload_size) {
    // Size check
    if (payload_size < sizeof(InputPacket)) {
        return;  // Too small
    }

    // Parse
    InputPacket input;
    std::memcpy(&input, payload, sizeof(InputPacket));

    // Optional: Sequence validation
    uint32_t last_seq = last_input_sequence_[player_id];
    if (input.sequence <= last_seq && last_seq - input.sequence < 1000) {
        // Out of order or duplicate (and not wrapped)
        return;
    }
    last_input_sequence_[player_id] = input.sequence;

    // Optional: Sanitize bits
    input.bits &= 0x3F;  // Mask unused bits

    // Store
    player_input_bits_[player_id] = input.bits;
}
```

## Wire Format Examples

### Example 1: Sequence=0, Moving Right

**Complete Message:**
```
Offset  Hex                      ASCII   Description
------  ----------------------   -----   -----------
0x0000  05 00 03 01              ....    Header (size=5, type=3, version=1)
0x0004  00 00 00 00              ....    sequence=0
0x0008  08                       .       bits=0x08 (InputRight)
```

**Total Size:** 9 bytes

### Example 2: Sequence=42, Moving Up+Right+Shooting

**Complete Message:**
```
Offset  Hex                      ASCII   Description
------  ----------------------   -----   -----------
0x0000  05 00 03 01              ....    Header
0x0004  2A 00 00 00              *...    sequence=42
0x0008  19                       .       bits=0x19 (Up|Right|Shoot = 0x01|0x08|0x10)
```

### Example 3: Sequence=1000, No Input (All Released)

**Complete Message:**
```
Offset  Hex                      ASCII   Description
------  ----------------------   -----   -----------
0x0000  05 00 03 01              ....    Header
0x0004  E8 03 00 00              ....    sequence=1000 (0x03E8)
0x0008  00                       .       bits=0x00 (no input)
```

## Common Issues

### Issue 1: Player Doesn't Move

**Problem:** Client sends Input but player doesn't respond

**Possible Causes:**
1. **Wrong bits:** Client sets wrong bitmask
2. **Server not receiving:** Network issue or wrong endpoint
3. **Player not created:** UDP Hello not successful
4. **Server bug:** Input not applied to movement

**Debug:**
```cpp
// Client: Log what you're sending
std::cout << "Sending Input: seq=" << seq << " bits=0x"
          << std::hex << (int)bits << "\n";

// Server: Log what you're receiving
std::cout << "Received Input from player " << player_id
          << ": bits=0x" << std::hex << (int)bits << "\n";
```

### Issue 2: Diagonal Movement Too Fast

**Problem:** Diagonal movement is faster than cardinal directions

**Cause:** Server doesn't normalize diagonal velocity

**Current Behavior:**
- Right: vx=150, vy=0, speed=150
- Up+Right: vx=150, vy=-150, speed=212 (√2 × 150)

**Fix (Server):**
```cpp
// Normalize diagonal movement
if (vx != 0 && vy != 0) {
    float len = std::sqrt(vx*vx + vy*vy);
    vx = (vx / len) * speed;
    vy = (vy / len) * speed;
}
```

### Issue 3: Input Lag

**Problem:** Player movement feels delayed

**Possible Causes:**
1. **High network latency:** RTT > 100ms
2. **Low send rate:** Client sends input at <30 Hz
3. **Server tick rate:** Server only processes at 60 Hz

**Solutions:**
- **Client-side prediction:** Move player locally before server confirms
- **Increase send rate:** Send input at 60 Hz
- **Interpolation:** Smooth movement between state updates

## Performance Characteristics

### Bandwidth Usage

**Per Client:**
- Message size: 9 bytes
- Send rate: 30-60 Hz
- Bandwidth: 270-540 bytes/sec = **2.2-4.3 Kbps**

**Server (N clients):**
- Receive: 2.2-4.3 Kbps × N

**Example (10 clients @ 60 Hz):**
- Total inbound: 43 Kbps (negligible)

### Latency

**Input-to-Display Pipeline:**
1. Client captures input: <1ms
2. Client sends packet: <1ms
3. Network transit: 1-100ms (varies)
4. Server receives and stores: <1ms
5. Server tick processes input: 0-16ms (depends on tick timing)
6. Server broadcasts State: <1ms
7. Network transit: 1-100ms
8. Client renders: <16ms

**Total:** ~20-250ms depending on network

## Testing

### Unit Test Example

```cpp
TEST(Protocol, Input_Serialization) {
    Header header{sizeof(InputPacket), MsgType::Input, ProtocolVersion};
    InputPacket payload{42, InputRight | InputShoot};

    uint8_t buffer[9];
    std::memcpy(buffer, &header, 4);
    std::memcpy(buffer + 4, &payload, 5);

    // Verify header
    EXPECT_EQ(buffer[0], 0x05);  // size = 5
    EXPECT_EQ(buffer[2], 3);     // type = Input

    // Verify payload
    EXPECT_EQ(buffer[4], 0x2A);  // sequence = 42 (low byte)
    EXPECT_EQ(buffer[8], 0x18);  // bits = 0x18
}

TEST(Protocol, InputBits) {
    uint8_t bits = InputUp | InputRight;
    EXPECT_EQ(bits, 0x09);
    EXPECT_TRUE(bits & InputUp);
    EXPECT_TRUE(bits & InputRight);
    EXPECT_FALSE(bits & InputLeft);
}
```

## Related Documentation

- **[udp-04-state.md](udp-04-state.md)** - State updates (server response to input)
- **[03-data-structures.md](03-data-structures.md)** - InputPacket definition
- **[00-overview.md](00-overview.md)** - Input-State loop explanation

---

**Last Updated:** 2025-10-29

