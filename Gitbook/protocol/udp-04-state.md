# State (4) - UDP

## Overview

**Message Type:** `State` (4)
**Transport:** UDP
**Direction:** Server → Client
**Purpose:** Broadcast complete world state to all clients
**Status:** Active
**Frequency:** ~60 Hz (every server tick)

## Message Format

### Complete Message Structure

```
┌─────────────────────────┬────────────────────────────────────┐
│   Header (4 bytes)      │  Payload (2 + N×25 bytes)          │
├─────────────────────────┼────────────────────────────────────┤
│ size=?, type=4, ver=1   │  StateHeader + N × PackedEntity    │
└─────────────────────────┴────────────────────────────────────┘
```

**Total Message Size:** 6 + (N × 25) bytes
- Header: 4 bytes
- StateHeader: 2 bytes
- PackedEntity: 25 bytes each
- **N:** Number of entities (0-512)

### Header

| Field | Value | Encoding |
|-------|-------|----------|
| `size` | 2 + (N × 25) | `uint16_t` (little-endian) |
| `type` | 4 | `uint8_t` (State) |
| `version` | 1 | `uint8_t` |

**Wire Format (Header):**
```
XX XX 04 01
└─ size (variable)
      └─ type=4 (State)
         └─ version=1
```

### Payload Structure

**Layout:**
```
┌──────────────┬──────────────┬──────────────┬─────┬──────────────┐
│ StateHeader  │ PackedEntity │ PackedEntity │ ... │ PackedEntity │
│  (2 bytes)   │  (25 bytes)  │  (25 bytes)  │     │  (25 bytes)  │
└──────────────┴──────────────┴──────────────┴─────┴──────────────┘
   count=N         Entity 0       Entity 1            Entity N-1
```

**Size Calculation:**
```cpp
size_t payload_size = sizeof(StateHeader) + (entity_count * sizeof(PackedEntity));
// payload_size = 2 + (N × 25)
```

## Data Structures

### StateHeader

**Structure:**
```cpp
#pragma pack(push, 1)
struct StateHeader {
    std::uint16_t count;  // Number of entities following
};
#pragma pack(pop)
```

**Size:** 2 bytes

**Field:**
- `count`: Number of `PackedEntity` structs following this header
- Range: 0-65535 (server limits to 512 in practice)
- Endianness: Little-endian

### PackedEntity

**Structure:**
```cpp
#pragma pack(push, 1)
struct PackedEntity {
    std::uint32_t id;       // Unique entity identifier
    EntityType type;        // Entity type (uint8_t: 1=Player, 2=Enemy, 3=Bullet)
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
| 0 | 4 | `uint32_t` | `id` | Entity ID (little-endian) |
| 4 | 1 | `EntityType` | `type` | 1=Player, 2=Enemy, 3=Bullet |
| 5 | 4 | `float` | `x` | X coordinate (IEEE-754 LE) |
| 9 | 4 | `float` | `y` | Y coordinate (IEEE-754 LE) |
| 13 | 4 | `float` | `vx` | X velocity (IEEE-754 LE) |
| 17 | 4 | `float` | `vy` | Y velocity (IEEE-754 LE) |
| 21 | 4 | `uint32_t` | `rgba` | Color 0xRRGGBBAA (LE) |

## Field Specifications

### count (StateHeader)

**Type:** `uint16_t` (2 bytes, little-endian)

**Purpose:** Number of entities in this state update

**Server Limit:** 512 entities maximum
```cpp
size_t count = std::min(entities.size(), size_t(512));
```

**Validation:**
- Client should verify: `payload_size >= 2 + (count × 25)`
- If mismatch, discard entire state packet

### id (PackedEntity)

**Type:** `uint32_t` (4 bytes, little-endian)

**Purpose:** Unique identifier for this entity

**Properties:**
- Assigned by server, never reused within a game session
- Stable across frames (same entity keeps same ID)
- Used by client to track entities over time

**Client Usage:**
```cpp
// Track entities by ID
std::unordered_map<uint32_t, Entity> entities_;

void handleState(const PackedEntity* entities, size_t count) {
    std::unordered_set<uint32_t> seen_ids;

    for (size_t i = 0; i < count; ++i) {
        uint32_t id = entities[i].id;
        seen_ids.insert(id);

        if (entities_.find(id) == entities_.end()) {
            // New entity
            entities_[id] = createEntity(entities[i]);
        } else {
            // Update existing entity
            updateEntity(entities_[id], entities[i]);
        }
    }

    // Remove entities no longer in state
    for (auto it = entities_.begin(); it != entities_.end(); ) {
        if (seen_ids.find(it->first) == seen_ids.end()) {
            it = entities_.erase(it);  // Entity despawned
        } else {
            ++it;
        }
    }
}
```

### type (PackedEntity)

**Type:** `EntityType` (1 byte)

**Values:**
```cpp
enum class EntityType : std::uint8_t {
    Player = 1,
    Enemy  = 2,
    Bullet = 3,
};
```

**Client Usage:**
- Determines rendering (sprite, animation, effects)
- Determines behavior (player control vs AI)
- May determine collision handling

### x, y (PackedEntity)

**Type:** `float` (4 bytes each, IEEE-754 little-endian)

**Purpose:** Entity position in world coordinates

**Coordinate System:**
- Origin: Top-left (0, 0)
- X-axis: Right is positive
- Y-axis: Down is positive (typical screen coordinates)
- Units: Pixels (typically)

**Range:** ±3.4 × 10^38 (float range), but game-specific limits apply

**Example:**
- Player spawn: (50.0, 100.0)
- Off-screen left: x < 0
- Off-screen right: x > screen_width

### vx, vy (PackedEntity)

**Type:** `float` (4 bytes each, IEEE-754 little-endian)

**Purpose:** Entity velocity in world units per second

**Usage:**
- Client can use for interpolation between states
- Client can use for prediction (extrapolation)
- Visual effects (particle direction, motion blur)

**Example Values:**
- Player moving right: vx=150.0, vy=0.0
- Enemy moving left: vx=-60.0, vy=0.0
- Stationary: vx=0.0, vy=0.0

**Interpolation Example:**
```cpp
// Smooth position between state updates
float lerp_factor = time_since_state / (1.0f / 60.0f);
float display_x = last_x + (current_x - last_x) * lerp_factor;

// Or extrapolate using velocity
float display_x = current_x + vx * time_since_state;
```

### rgba (PackedEntity)

**Type:** `uint32_t` (4 bytes, little-endian)

**Purpose:** Entity color for rendering

**Format:** `0xRRGGBBAA`
- RR: Red (0x00-0xFF)
- GG: Green (0x00-0xFF)
- BB: Blue (0x00-0xFF)
- AA: Alpha (0x00-0xFF, 0xFF=fully opaque)

**Example Colors:**
- Red player: `0xFF0000FF` (255, 0, 0, 255)
- Blue player: `0x55AAFFFF` (85, 170, 255, 255)
- White enemy: `0xFFFFFFFF` (255, 255, 255, 255)
- Semi-transparent: `0xFF000080` (255, 0, 0, 128)

**Wire Format (0x55AAFFFF):**
```
FF FF AA 55
└─ RGBA bytes in little-endian (AA, BB, GG, RR from right to left)
```

**Parsing:**
```cpp
uint32_t rgba = 0x55AAFFFF;
uint8_t r = (rgba >> 0) & 0xFF;   // 0xFF
uint8_t g = (rgba >> 8) & 0xFF;   // 0xFF
uint8_t b = (rgba >> 16) & 0xFF;  // 0xAA
uint8_t a = (rgba >> 24) & 0xFF;  // 0x55
```

## Protocol Flow

### State Broadcast

```
Server                               Clients
   │                                 │  │  │
   │ [Tick 60 Hz]                    │  │  │
   │ [Process inputs]                │  │  │
   │ [Update physics]                │  │  │
   │ [Spawn/despawn entities]        │  │  │
   │                                 │  │  │
   │──── State (N entities) ─────────>│  │  │
   │──── State (N entities) ─────────────>│  │
   │──── State (N entities) ──────────────────>│
   │                                 │  │  │
   │ [Clients render]                ▼  ▼  ▼
   │                                 │  │  │
   │ [16.67ms later, next tick]      │  │  │
   │                                 │  │  │
   │──── State (N entities) ─────────>│  │  │
   │                                 │  │  │
```

**Broadcast:** Server sends same State packet to all connected clients every tick (~16.67ms).

## Server Behavior

### Building State Message

**Example Implementation:**
```cpp
void broadcastState() {
    // Limit entity count
    size_t count = std::min(entities_.size(), size_t(512));

    // Calculate sizes
    size_t payload_size = sizeof(StateHeader) + count * sizeof(PackedEntity);

    // Allocate buffer
    std::vector<uint8_t> buffer(sizeof(Header) + payload_size);

    // Write header
    Header* header = reinterpret_cast<Header*>(buffer.data());
    header->size = payload_size;
    header->type = MsgType::State;
    header->version = ProtocolVersion;

    // Write state header
    StateHeader* state_hdr = reinterpret_cast<StateHeader*>(buffer.data() + sizeof(Header));
    state_hdr->count = count;

    // Write entities
    PackedEntity* entities_arr = reinterpret_cast<PackedEntity*>(
        buffer.data() + sizeof(Header) + sizeof(StateHeader)
    );

    size_t idx = 0;
    for (auto& [id, entity] : entities_) {
        if (idx >= 512) break;

        entities_arr[idx].id = id;
        entities_arr[idx].type = entity.type;
        entities_arr[idx].x = entity.x;
        entities_arr[idx].y = entity.y;
        entities_arr[idx].vx = entity.vx;
        entities_arr[idx].vy = entity.vy;
        entities_arr[idx].rgba = entity.rgba;

        ++idx;
    }

    // Send to all clients
    for (auto& [endpoint, player_id] : clients_) {
        socket_.send_to(asio::buffer(buffer), endpoint);
    }
}
```

## Client Behavior

### Receiving and Parsing State

**Example Implementation:**
```cpp
void handleState(const uint8_t* payload, size_t payload_size) {
    // Validate minimum size
    if (payload_size < sizeof(StateHeader)) {
        return;  // Invalid
    }

    // Parse state header
    StateHeader state_hdr;
    std::memcpy(&state_hdr, payload, sizeof(StateHeader));

    // Validate size
    size_t expected_size = sizeof(StateHeader) + state_hdr.count * sizeof(PackedEntity);
    if (payload_size < expected_size) {
        return;  // Truncated
    }

    // Parse entities
    const uint8_t* entities_data = payload + sizeof(StateHeader);

    for (uint16_t i = 0; i < state_hdr.count; ++i) {
        PackedEntity entity;
        std::memcpy(&entity, entities_data + i * sizeof(PackedEntity), sizeof(PackedEntity));

        // Process entity
        updateOrCreateEntity(entity);
    }

    // Remove entities not in this state
    pruneOldEntities(state_hdr);
}
```

### Client Rendering

**Naive Approach (Direct Rendering):**
```cpp
void render() {
    for (auto& [id, entity] : entities_) {
        drawSprite(entity.type, entity.x, entity.y, entity.rgba);
    }
}
```

**Improved (Interpolation):**
```cpp
void render(float lerp_factor) {
    for (auto& [id, entity] : entities_) {
        float display_x = lerp(entity.prev_x, entity.x, lerp_factor);
        float display_y = lerp(entity.prev_y, entity.y, lerp_factor);
        drawSprite(entity.type, display_x, display_y, entity.rgba);
    }
}
```

**Advanced (Extrapolation):**
```cpp
void render(float dt_since_state) {
    for (auto& [id, entity] : entities_) {
        float display_x = entity.x + entity.vx * dt_since_state;
        float display_y = entity.y + entity.vy * dt_since_state;
        drawSprite(entity.type, display_x, display_y, entity.rgba);
    }
}
```

## Validation

### Server Validation (Before Sending)

**Required:**
- [ ] Entity count ≤ 512
- [ ] All entity IDs are valid
- [ ] All entity positions are reasonable (not NaN, not Inf)

### Client Validation (After Receiving)

**Required:**
- [ ] `header.version == 1`
- [ ] `header.type == 4` (State)
- [ ] `payload_size >= 2`
- [ ] `payload_size >= 2 + (count × 25)`

**Optional:**
- [ ] Entity count is reasonable (e.g., < 1000)
- [ ] Entity types are valid (1, 2, or 3)
- [ ] Positions and velocities are not NaN or Inf

**Example:**
```cpp
bool validateState(const Header& header, const uint8_t* payload, size_t payload_size) {
    // Basic checks
    if (header.version != ProtocolVersion) return false;
    if (header.type != MsgType::State) return false;
    if (payload_size < sizeof(StateHeader)) return false;

    // Parse count
    StateHeader state_hdr;
    std::memcpy(&state_hdr, payload, sizeof(StateHeader));

    // Size check
    size_t expected_size = sizeof(StateHeader) + state_hdr.count * sizeof(PackedEntity);
    if (payload_size < expected_size) {
        std::cerr << "State size mismatch: expected " << expected_size
                  << " got " << payload_size << "\n";
        return false;
    }

    // Sanity check on count
    if (state_hdr.count > 1000) {
        std::cerr << "Suspicious entity count: " << state_hdr.count << "\n";
        return false;
    }

    return true;
}
```

## Wire Format Examples

### Example 1: Empty State (No Entities)

**Complete Message:**
```
Offset  Hex              ASCII   Description
------  --------------   -----   -----------
0x0000  02 00 04 01      ....    Header (size=2, type=4, version=1)
0x0004  00 00            ..      StateHeader (count=0)
```

**Total Size:** 6 bytes

### Example 2: Single Player Entity

**Complete Message:**
```
Offset  Hex                                              Description
------  ----------------------------------------------   -----------
0x0000  1B 00 04 01                                      Header (size=27)
0x0004  01 00                                            StateHeader (count=1)
0x0006  01 00 00 00                                      id=1
0x000A  01                                               type=Player
0x000B  00 00 48 42                                      x=50.0
0x000F  00 00 C8 42                                      y=100.0
0x0013  00 00 00 00                                      vx=0.0
0x0017  00 00 00 00                                      vy=0.0
0x001B  FF FF AA 55                                      rgba=0x55AAFFFF
```

**Total Size:** 31 bytes (6 header/state + 25 entity)

### Example 3: Two Entities (Player + Enemy)

**Total Size:** 56 bytes
- Header: 4 bytes
- StateHeader: 2 bytes
- Entity 1 (Player): 25 bytes
- Entity 2 (Enemy): 25 bytes

## Performance and Bandwidth

### Message Size

**Per Entity:** 25 bytes

**Examples:**
| Entities | Payload | Total | Bandwidth @ 60 Hz |
|----------|---------|-------|-------------------|
| 0 | 2 | 6 | 360 B/s = 2.9 Kbps |
| 10 | 252 | 256 | 15.4 KB/s = 123 Kbps |
| 50 | 1,252 | 1,256 | 75.4 KB/s = 603 Kbps |
| 100 | 2,502 | 2,506 | 150.4 KB/s = 1.2 Mbps |
| 512 | 12,802 | 12,806 | 768.4 KB/s = 6.1 Mbps |

**Server Bandwidth (N clients):**
- With 10 entities: 123 Kbps × N clients
- Example: 10 clients = 1.23 Mbps outbound
- Example: 100 clients = 12.3 Mbps outbound

### MTU Considerations

**Standard MTU:** ~1500 bytes (Ethernet)

**UDP Header:** 8 bytes

**Available:** ~1492 bytes per packet

**Max Entities per Packet (No Fragmentation):**
```
(1492 - 4 - 2) / 25 = 59 entities
```

**With 512 Entities:**
- Packet size: 12,806 bytes
- Exceeds MTU: Yes (requires IP fragmentation)
- Fragmentation risk: If any fragment lost, entire packet lost

**Recommendation:** Limit entities per State to ~50 or implement delta compression.

## Common Issues

### Issue 1: Entity Flickering

**Problem:** Entities appear/disappear randomly

**Causes:**
1. **Packet Loss:** State messages lost due to UDP unreliability
2. **Parsing Errors:** Client fails to parse some states
3. **ID Reuse:** Server reuses entity IDs too quickly

**Solutions:**
```cpp
// Client: Keep entity alive briefly after disappearing from state
const float ENTITY_TIMEOUT = 0.5f;  // 500ms

void updateEntity(const PackedEntity& state) {
    entities_[state.id].last_seen = current_time;
    // ... update fields
}

void pruneEntities() {
    for (auto it = entities_.begin(); it != entities_.end(); ) {
        if (current_time - it->second.last_seen > ENTITY_TIMEOUT) {
            it = entities_.erase(it);
        } else {
            ++it;
        }
    }
}
```

### Issue 2: Jittery Movement

**Problem:** Entity movement is not smooth

**Causes:**
1. **No Interpolation:** Directly rendering received positions
2. **Packet Loss:** Missing intermediate states
3. **Variable Latency:** Jitter in packet arrival times

**Solutions:**
```cpp
// Interpolation between states
void update(float dt) {
    lerp_progress_ += dt / (1.0f / 60.0f);
    lerp_progress_ = std::min(lerp_progress_, 1.0f);
}

void render() {
    for (auto& [id, entity] : entities_) {
        float x = lerp(entity.prev_x, entity.target_x, lerp_progress_);
        float y = lerp(entity.prev_y, entity.target_y, lerp_progress_);
        draw(x, y);
    }
}

void onStateReceived(const PackedEntity& state) {
    auto& entity = entities_[state.id];
    entity.prev_x = entity.target_x;
    entity.prev_y = entity.target_y;
    entity.target_x = state.x;
    entity.target_y = state.y;
    lerp_progress_ = 0.0f;
}
```

### Issue 3: High Bandwidth Usage

**Problem:** State messages consume too much bandwidth

**Current:** 10 entities = 123 Kbps per client

**Solutions:**
1. **Reduce Tick Rate:** 30 Hz instead of 60 Hz (halves bandwidth)
2. **Interest Management:** Only send nearby entities
3. **Delta Compression:** Only send changed entities (use Spawn/Despawn)
4. **Quantization:** Use int16 positions instead of float (not compatible with current protocol)

## Testing

### Unit Test Example

```cpp
TEST(Protocol, State_Serialization) {
    // Build state with 2 entities
    StateHeader state_hdr{2};
    PackedEntity entities[2] = {
        {1, EntityType::Player, 50.0f, 100.0f, 0.0f, 0.0f, 0xFF0000FF},
        {2, EntityType::Enemy, 200.0f, 150.0f, -60.0f, 0.0f, 0xFFFFFFFF}
    };

    size_t payload_size = sizeof(StateHeader) + 2 * sizeof(PackedEntity);
    Header header{(uint16_t)payload_size, MsgType::State, ProtocolVersion};

    // Serialize
    std::vector<uint8_t> buffer(sizeof(Header) + payload_size);
    std::memcpy(buffer.data(), &header, sizeof(Header));
    std::memcpy(buffer.data() + sizeof(Header), &state_hdr, sizeof(StateHeader));
    std::memcpy(buffer.data() + sizeof(Header) + sizeof(StateHeader), entities, 2 * sizeof(PackedEntity));

    // Verify size
    EXPECT_EQ(buffer.size(), 6 + 50);  // 4 + 2 + 25*2

    // Verify header
    EXPECT_EQ(buffer[2], 4);  // type = State

    // Verify count
    uint16_t count;
    std::memcpy(&count, buffer.data() + 4, 2);
    EXPECT_EQ(count, 2);
}
```

## Related Documentation

- **[udp-03-input.md](udp-03-input.md)** - Input messages (trigger state updates)
- **[03-data-structures.md](03-data-structures.md)** - PackedEntity, StateHeader definitions
- **[00-overview.md](00-overview.md)** - Input-State loop explanation

---

**Last Updated:** 2025-10-29

