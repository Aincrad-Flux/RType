# Despawn (6) - UDP

## Overview

**Message Type:** `Despawn` (6)
**Transport:** UDP
**Direction:** Server → Client
**Purpose:** Notify clients of entity removal (delta update alternative to full State)
**Status:** ⚠️ **RESERVED** (not currently implemented)

## Current Status

This message type is defined in the protocol but **not currently used** by the server. It is reserved for future implementation of delta-based state updates.

**Current Approach:** Server sends complete world state via `State` messages. Clients detect entity removal when an entity ID disappears from the State.

**Future Approach (with Despawn):** Server explicitly notifies clients when entities are removed, allowing for:
- Reduced bandwidth (don't send full state every tick)
- Explicit death/removal events
- Client-side effects (explosion animation, sound)

## Proposed Message Format

### Complete Message Structure

```
┌─────────────────────────┬───────────────────────┐
│   Header (4 bytes)      │  Payload (4 bytes)    │
├─────────────────────────┼───────────────────────┤
│ size=4, type=6, ver=1   │  Entity ID            │
└─────────────────────────┴───────────────────────┘
```

**Total Message Size:** 8 bytes

### Header

| Field | Value | Encoding |
|-------|-------|----------|
| `size` | 4 | `uint16_t` (little-endian) |
| `type` | 6 | `uint8_t` (Despawn) |
| `version` | 1 | `uint8_t` |

### Payload: Entity ID

**Structure:**
```cpp
struct DespawnPayload {
    std::uint32_t entity_id;  // ID of entity to remove
};
```

**Size:** 4 bytes

**Binary Layout:**

| Offset | Size | Type | Field | Description |
|--------|------|------|-------|-------------|
| 0 | 4 bytes | `uint32_t` | `entity_id` | Entity ID to remove (little-endian) |

## Field Specifications

### entity_id

**Type:** `uint32_t` (4 bytes, little-endian)

**Purpose:** Identifies which entity should be removed from the client's local state

**Usage:**
- Must match an existing entity ID on the client
- If ID not found, client should silently ignore (entity may have already been removed)

## Proposed Protocol Flow

### Entity Removal

```
Client                                Server
   │                                     │
   │ [Entity 42 exists locally]          │
   │                                     │
   │              [Enemy 42 dies/exits]  │
   │              [Server removes entity]│
   │                                     │
   │<──── Despawn (id=42) ──────────────────│
   │                                     │
   │ [Remove entity 42 locally]          │
   │ [Play death animation]              │
   │ [Play sound effect]                 │
   │                                     │
```

**Timing:**
- Server sends immediately when entity is destroyed or leaves play area
- Client processes and removes entity from local state
- Client may delay actual removal to play animations

## Proposed Server Behavior

### When to Send Despawn

Server should send `Despawn` when:
1. Entity is destroyed (e.g., enemy killed, bullet hits target)
2. Entity leaves play area (e.g., enemy exits screen left, bullet goes off screen)
3. Entity times out (e.g., temporary power-up expires)
4. Entity leaves client's interest area (if using spatial culling)

**Example Implementation:**
```cpp
void removeEntity(uint32_t entity_id) {
    // Remove from server state
    auto it = entities_.find(entity_id);
    if (it == entities_.end()) {
        return;  // Already removed
    }

    entities_.erase(it);

    // Notify all clients
    broadcastDespawn(entity_id);
}

void broadcastDespawn(uint32_t entity_id) {
    Header header{sizeof(uint32_t), MsgType::Despawn, ProtocolVersion};

    uint8_t buffer[sizeof(Header) + sizeof(uint32_t)];
    std::memcpy(buffer, &header, sizeof(Header));
    std::memcpy(buffer + sizeof(Header), &entity_id, sizeof(uint32_t));

    for (auto& [endpoint, player_id] : clients_) {
        socket_.send_to(asio::buffer(buffer, sizeof(buffer)), endpoint);
    }
}
```

### Batch Despawn

For efficiency, server could batch multiple despawns:

```cpp
struct DespawnBatchPayload {
    std::uint16_t count;
    std::uint32_t entity_ids[];  // Variable length
};

void broadcastDespawnBatch(const std::vector<uint32_t>& ids) {
    size_t payload_size = sizeof(uint16_t) + ids.size() * sizeof(uint32_t);
    Header header{(uint16_t)payload_size, MsgType::Despawn, ProtocolVersion};

    // ... serialize and send
}
```

**Note:** This would require protocol version change or new message type.

## Proposed Client Behavior

### On Receiving Despawn

```cpp
void handleDespawn(const uint8_t* payload, size_t payload_size) {
    // Validate size
    if (payload_size != sizeof(uint32_t)) {
        return;  // Invalid
    }

    // Parse entity ID
    uint32_t entity_id;
    std::memcpy(&entity_id, payload, sizeof(uint32_t));

    // Find entity
    auto it = entities_.find(entity_id);
    if (it == entities_.end()) {
        // Entity not found (already removed, or never existed)
        // This is OK - silently ignore
        return;
    }

    // Optional: Play death effects based on entity type
    playDespawnEffects(it->second);

    // Remove entity
    entities_.erase(it);
}

void playDespawnEffects(const Entity& entity) {
    switch (entity.type) {
        case EntityType::Enemy:
            playSound("explosion.wav");
            spawnParticleEffect(entity.x, entity.y, "explosion");
            break;
        case EntityType::Bullet:
            spawnParticleEffect(entity.x, entity.y, "hit_spark");
            break;
        case EntityType::Player:
            playSound("player_death.wav");
            spawnParticleEffect(entity.x, entity.y, "player_explosion");
            break;
    }
}
```

### Delayed Removal for Animation

Client may want to keep entity visible briefly for death animation:

```cpp
struct Entity {
    // ... entity fields
    bool marked_for_removal = false;
    float removal_timer = 0.0f;
};

void handleDespawn(uint32_t entity_id) {
    auto it = entities_.find(entity_id);
    if (it == entities_.end()) return;

    // Mark for removal, but keep alive for animation
    it->second.marked_for_removal = true;
    it->second.removal_timer = 1.0f;  // 1 second for death animation

    playDespawnEffects(it->second);
}

void update(float dt) {
    for (auto it = entities_.begin(); it != entities_.end(); ) {
        if (it->second.marked_for_removal) {
            it->second.removal_timer -= dt;
            if (it->second.removal_timer <= 0.0f) {
                it = entities_.erase(it);
                continue;
            }
        }
        ++it;
    }
}
```

## Validation

### Server Validation (Before Sending)

**Required:**
- [ ] Entity ID exists in server state
- [ ] Entity ID has not been despawned already in this tick

### Client Validation (After Receiving)

**Required:**
- [ ] `header.version == 1`
- [ ] `header.type == 6` (Despawn)
- [ ] `header.size == 4`
- [ ] `payload_size >= 4`

**Optional:**
- [ ] Entity ID exists in client state (if not, silently ignore)

**Example:**
```cpp
bool validateDespawn(const Header& header, const uint8_t* payload, size_t payload_size) {
    // Version check
    if (header.version != ProtocolVersion) {
        return false;
    }

    // Type check
    if (header.type != MsgType::Despawn) {
        return false;
    }

    // Size check
    if (header.size != 4 || payload_size < 4) {
        std::cerr << "Despawn size mismatch\n";
        return false;
    }

    return true;
}
```

## Wire Format Example

### Despawn Message for Entity ID 42

**Complete Message:**
```
Offset  Hex              ASCII   Description
------  --------------   -----   -----------
0x0000  04 00 06 01      ....    Header (size=4, type=6, version=1)
0x0004  2A 00 00 00      *...    entity_id=42 (0x0000002A)
```

**Total Size:** 8 bytes

### Multiple Despawns

Server must send separate messages for each despawned entity:

**Despawn Entity 10:**
```
04 00 06 01 0A 00 00 00
```

**Despawn Entity 11:**
```
04 00 06 01 0B 00 00 00
```

**Total:** 16 bytes for 2 entities

## Common Issues

### Issue 1: Entity Never Removed

**Problem:** Client receives Despawn but entity still visible

**Causes:**
1. **Parse Error:** Client fails to parse entity_id correctly
2. **Wrong ID:** Server sends wrong entity ID
3. **Endianness:** Byte order mismatch

**Debug:**
```cpp
// Client: Log received despawns
std::cout << "Despawn received: entity_id=" << entity_id << "\n";

// Server: Log sent despawns
std::cout << "Despawn sent: entity_id=" << entity_id << "\n";
```

### Issue 2: Entity Disappears Instantly (No Animation)

**Problem:** Entity removed immediately without visual feedback

**Solution:** Implement delayed removal (see "Delayed Removal for Animation" above)

### Issue 3: Despawn Before Spawn

**Problem:** Client receives Despawn for entity it never received Spawn for

**Causes:**
1. **Packet Loss:** Spawn message was lost
2. **Out-of-Order:** Despawn arrived before Spawn
3. **Client Joined Late:** Entity spawned before client connected

**Solution:**
```cpp
void handleDespawn(uint32_t entity_id) {
    auto it = entities_.find(entity_id);
    if (it == entities_.end()) {
        // Entity not found - this is OK
        // May have missed Spawn, or entity was short-lived
        return;
    }

    // Remove entity
    entities_.erase(it);
}
```

## Performance Characteristics

### Bandwidth

**Per Despawn:** 8 bytes

**Scenarios:**
- 1 enemy despawns per 2 seconds: 8 bytes / 2s = 4 bytes/s
- 10 bullets despawn per second: 80 bytes/s

**Comparison to Full State:**
- Full State (10 entities): 256 bytes @ 60 Hz = 15.4 KB/s
- Despawn: 8 bytes per removal event (negligible)

### Latency

- Single UDP datagram (no fragmentation)
- Client processes immediately
- Animation delay is client-side choice

## Use Cases

### Enemy Defeated

```cpp
// Server: Enemy health reaches 0
void onEnemyDeath(uint32_t enemy_id) {
    removeEntity(enemy_id);  // Sends Despawn
    incrementPlayerScore(killer_player_id);
}
```

### Bullet Hits Target

```cpp
// Server: Bullet collision detected
void onBulletHit(uint32_t bullet_id) {
    removeEntity(bullet_id);  // Sends Despawn
    dealDamage(hit_entity_id);
}
```

### Entity Leaves Screen

```cpp
// Server: Entity x coordinate beyond screen bounds
void tick() {
    for (auto it = entities_.begin(); it != entities_.end(); ) {
        if (it->second.x < -50 || it->second.x > screen_width + 50) {
            uint32_t id = it->first;
            it = entities_.erase(it);
            broadcastDespawn(id);
        } else {
            ++it;
        }
    }
}
```

## Testing

### Unit Test Example

```cpp
TEST(Protocol, Despawn_Format) {
    Header header{4, MsgType::Despawn, ProtocolVersion};
    uint32_t entity_id = 42;

    uint8_t buffer[8];
    std::memcpy(buffer, &header, 4);
    std::memcpy(buffer + 4, &entity_id, 4);

    // Verify header
    EXPECT_EQ(buffer[0], 0x04);  // size = 4
    EXPECT_EQ(buffer[2], 6);     // type = Despawn

    // Verify entity_id
    EXPECT_EQ(buffer[4], 0x2A);  // 42 low byte
    EXPECT_EQ(buffer[5], 0x00);
    EXPECT_EQ(buffer[6], 0x00);
    EXPECT_EQ(buffer[7], 0x00);
}

TEST(Protocol, Despawn_ClientHandling) {
    // Setup
    Client client;
    client.addEntity(42, EntityType::Enemy, 100, 100);

    // Send Despawn
    client.handleDespawn(42);

    // Verify entity removed
    EXPECT_FALSE(client.hasEntity(42));
}
```

### Integration Test

```cpp
TEST(Protocol, Spawn_Despawn_Cycle) {
    Server server;
    Client client;

    // Spawn entity
    uint32_t id = server.spawnEnemy(100, 100);
    client.processMessages();
    ASSERT_TRUE(client.hasEntity(id));

    // Despawn entity
    server.removeEntity(id);
    client.processMessages();
    EXPECT_FALSE(client.hasEntity(id));
}
```

## Implementation Checklist

To implement Despawn support:

**Server:**
- [ ] Send Despawn when entity is removed
- [ ] Track despawned IDs to avoid double-despawn
- [ ] Consider batching multiple despawns
- [ ] Log despawns for debugging

**Client:**
- [ ] Handle Despawn message type
- [ ] Remove entity from local state
- [ ] Handle missing entity ID gracefully
- [ ] Optional: Implement death animations
- [ ] Optional: Play sound effects

**Testing:**
- [ ] Test with entity that never existed
- [ ] Test with already-removed entity (duplicate Despawn)
- [ ] Test with packet loss
- [ ] Test visual effects and animations

## Related Documentation

- **[udp-05-spawn.md](udp-05-spawn.md)** - Spawn message (entity creation)
- **[udp-04-state.md](udp-04-state.md)** - State message (current full-state approach)
- **[03-data-structures.md](03-data-structures.md)** - Entity structure definitions

---

**Last Updated:** 2025-10-29
**Status:** ⚠️ Reserved for future implementation

