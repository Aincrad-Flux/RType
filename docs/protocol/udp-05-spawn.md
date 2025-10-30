# Spawn (5) - UDP

## Overview

**Message Type:** `Spawn` (5)
**Transport:** UDP
**Direction:** Server → Client
**Purpose:** Notify clients of new entity creation (delta update alternative to full State)
**Status:** ⚠️ **RESERVED** (not currently implemented)

## Current Status

This message type is defined in the protocol but **not currently used** by the server. It is reserved for future implementation of delta-based state updates.

**Current Approach:** Server sends complete world state via `State` messages at 60 Hz.

**Future Approach (with Spawn):** Server sends:
- Full `State` periodically (e.g., every 1 second or on client join)
- `Spawn` messages for new entities
- `Despawn` messages for removed entities
- Optionally: Update messages for changed entities

**Benefits:**
- Reduced bandwidth (only send changes, not full state)
- More scalable for large entity counts
- Better for games with many static entities

## Proposed Message Format

### Complete Message Structure

```
┌─────────────────────────┬───────────────────────┐
│   Header (4 bytes)      │  Payload (25 bytes)   │
├─────────────────────────┼───────────────────────┤
│ size=25, type=5, ver=1  │  PackedEntity         │
└─────────────────────────┴───────────────────────┘
```

**Total Message Size:** 29 bytes

### Header

| Field | Value | Encoding |
|-------|-------|----------|
| `size` | 25 | `uint16_t` (little-endian) |
| `type` | 5 | `uint8_t` (Spawn) |
| `version` | 1 | `uint8_t` |

### Payload: PackedEntity

**Structure:** Same as in State messages

```cpp
#pragma pack(push, 1)
struct PackedEntity {
    std::uint32_t id;       // Unique entity identifier
    EntityType type;        // Entity type (1=Player, 2=Enemy, 3=Bullet)
    float x, y;             // Position
    float vx, vy;           // Velocity
    std::uint32_t rgba;     // Color
};
#pragma pack(pop)
```

**Size:** 25 bytes

See [03-data-structures.md](03-data-structures.md) for full field specifications.

## Proposed Protocol Flow

### Delta-Based State Updates

```
Client                                Server
   │                                     │
   │<──── State (full state) ─────────────│  Initial sync
   │                                     │
   │                    [New enemy spawns]│
   │                                     │
   │<──── Spawn (enemy entity) ────────────│  Delta update
   │                                     │
   │ [Create entity locally]             │
   │                                     │
   │                [Player shoots bullet]│
   │                                     │
   │<──── Spawn (bullet entity) ───────────│  Delta update
   │                                     │
   │                 [Enemy reaches edge] │
   │                                     │
   │<──── Despawn (enemy ID) ──────────────│  Delta update
   │                                     │
   │ [Remove entity locally]             │
   │                                     │
```

## Proposed Server Behavior

### When to Send Spawn

Server should send `Spawn` when:
1. New entity created
2. Entity enters client's interest area (if using spatial culling)
3. Client joins and needs initial entities (could use full State instead)

**Example Implementation:**
```cpp
void createEntity(EntityType type, float x, float y) {
    uint32_t id = next_entity_id_++;

    Entity entity;
    entity.id = id;
    entity.type = type;
    entity.x = x;
    entity.y = y;
    // ... initialize other fields

    entities_[id] = entity;

    // Send Spawn to all clients
    broadcastSpawn(entity);
}

void broadcastSpawn(const Entity& entity) {
    Header header{sizeof(PackedEntity), MsgType::Spawn, ProtocolVersion};

    PackedEntity packed;
    packed.id = entity.id;
    packed.type = entity.type;
    packed.x = entity.x;
    packed.y = entity.y;
    packed.vx = entity.vx;
    packed.vy = entity.vy;
    packed.rgba = entity.rgba;

    uint8_t buffer[sizeof(Header) + sizeof(PackedEntity)];
    std::memcpy(buffer, &header, sizeof(Header));
    std::memcpy(buffer + sizeof(Header), &packed, sizeof(PackedEntity));

    for (auto& [endpoint, player_id] : clients_) {
        socket_.send_to(asio::buffer(buffer, sizeof(buffer)), endpoint);
    }
}
```

## Proposed Client Behavior

### On Receiving Spawn

```cpp
void handleSpawn(const uint8_t* payload, size_t payload_size) {
    // Validate size
    if (payload_size != sizeof(PackedEntity)) {
        return;  // Invalid
    }

    // Parse entity
    PackedEntity entity;
    std::memcpy(&entity, payload, sizeof(PackedEntity));

    // Check if entity already exists
    if (entities_.find(entity.id) != entities_.end()) {
        // Already exists, might be duplicate packet or out-of-order
        // Update instead of creating
        updateEntity(entities_[entity.id], entity);
    } else {
        // New entity, create it
        entities_[entity.id] = createEntity(entity);
    }
}
```

## Advantages of Delta Updates

### Bandwidth Comparison

**Full State Approach (Current):**
- 10 entities: 256 bytes @ 60 Hz = 15.4 KB/s per client
- No change: Still sending full state

**Delta Approach (With Spawn/Despawn):**
- Initial: 256 bytes (full state)
- Spawn: 29 bytes per new entity
- Despawn: ~8 bytes per removed entity
- If 1 enemy spawns per 2 seconds: 29 bytes / 2s = 14.5 bytes/s
- **Savings:** 99% reduction for stable scenes

**Example Scenario:**
- 10 static players
- 1 enemy spawns every 2 seconds
- 1 bullet spawned per second (average)

**Full State:** 256 bytes × 60 Hz = 15.4 KB/s

**Delta:**
- Full State: 256 bytes @ 0.1 Hz (every 10s for sync) = 25.6 bytes/s
- Spawn (enemy): 29 bytes @ 0.5 Hz = 14.5 bytes/s
- Spawn (bullet): 29 bytes @ 1 Hz = 29 bytes/s
- Total: ~69 bytes/s = **0.6 KB/s (4.5% of full state bandwidth)**

## Challenges and Considerations

### Reliability

**Problem:** UDP is unreliable; Spawn packets may be lost

**Solutions:**
1. **Periodic Full State:** Send full State every N seconds as sync point
2. **Retransmit on Request:** Client requests missing entities
3. **Redundancy:** Server sends Spawn multiple times
4. **Sequence Numbers:** Detect gaps and request resync

### Out-of-Order Delivery

**Problem:** Spawn may arrive after Despawn for same entity

**Solutions:**
1. **Sequence Numbers:** Include per-entity or global sequence number
2. **Timestamps:** Include spawn time, ignore old messages
3. **State Machine:** Track entity lifecycle, ignore invalid transitions

### Initial Synchronization

**Problem:** New client needs all existing entities

**Solutions:**
1. **Full State on Join:** Send complete State message
2. **Spawn Burst:** Send Spawn for each entity
3. **Reliable TCP:** Send initial entities via TCP

## Implementation Checklist

To implement Spawn support:

**Server:**
- [ ] Track entity creation events
- [ ] Send Spawn message on entity creation
- [ ] Implement periodic full State sync (e.g., every 10 seconds)
- [ ] Consider spatial interest management

**Client:**
- [ ] Handle Spawn message type
- [ ] Create entity from PackedEntity data
- [ ] Handle duplicate Spawn (idempotent)
- [ ] Handle out-of-order Spawn/Despawn
- [ ] Request resync if state seems inconsistent

**Testing:**
- [ ] Test with packet loss simulation
- [ ] Test with high spawn rate
- [ ] Test with new client joining mid-game
- [ ] Test out-of-order packet delivery

## Wire Format Example

### Spawn Message for Enemy Entity

**Complete Message:**
```
Offset  Hex                                              Description
------  ----------------------------------------------   -----------
0x0000  19 00 05 01                                      Header (size=25, type=5, version=1)
0x0004  0A 00 00 00                                      id=10
0x0008  02                                               type=Enemy
0x0009  00 00 61 43                                      x=225.0
0x000D  00 00 48 42                                      y=50.0
0x0011  00 00 70 C2                                      vx=-60.0
0x0015  00 00 00 00                                      vy=0.0
0x0019  FF FF FF FF                                      rgba=0xFFFFFFFF (white)
```

**Total Size:** 29 bytes

## Testing Without Implementation

To test client Spawn handling before server implementation:

**Mock Spawn Generator:**
```cpp
// Test client Spawn handling
void sendMockSpawn(uint32_t id, float x, float y) {
    Header header{sizeof(PackedEntity), MsgType::Spawn, ProtocolVersion};

    PackedEntity entity{id, EntityType::Enemy, x, y, -60.0f, 0.0f, 0xFFFFFFFF};

    uint8_t buffer[29];
    std::memcpy(buffer, &header, 4);
    std::memcpy(buffer + 4, &entity, 25);

    // Send to client
    socket.send_to(asio::buffer(buffer), client_endpoint);
}
```

## Migration Path

### Phase 1: Hybrid Mode
- Keep full State at 60 Hz
- Add Spawn messages (redundant)
- Test client handling

### Phase 2: Reduced State
- Send full State at 10 Hz
- Use Spawn for new entities
- Monitor for desyncs

### Phase 3: Full Delta
- Send full State on join only
- All updates via Spawn/Despawn/Update messages
- Periodic State for error correction

## Related Documentation

- **[udp-06-despawn.md](udp-06-despawn.md)** - Despawn message (entity removal)
- **[udp-04-state.md](udp-04-state.md)** - State message (full state sync)
- **[03-data-structures.md](03-data-structures.md)** - PackedEntity definition

---

**Last Updated:** 2025-10-29
**Status:** ⚠️ Reserved for future implementation

