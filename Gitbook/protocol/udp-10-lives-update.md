# LivesUpdate (10) - UDP

## Overview

**Message Type:** `LivesUpdate` (10)
**Transport:** UDP
**Direction:** Server → Client
**Purpose:** Notify clients of a single player's lives change
**Status:** Active
**Frequency:** On event (when any player loses or gains a life)

## Message Format

### Complete Message Structure

```
┌─────────────────────────┬───────────────────────┐
│   Header (4 bytes)      │  Payload (5 bytes)    │
├─────────────────────────┼───────────────────────┤
│ size=5, type=10, ver=1  │  LivesUpdatePayload   │
└─────────────────────────┴───────────────────────┘
```

**Total Message Size:** 9 bytes

### Header

| Field | Value | Encoding |
|-------|-------|----------|
| `size` | 5 | `uint16_t` (little-endian) |
| `type` | 10 | `uint8_t` (LivesUpdate) |
| `version` | 1 | `uint8_t` |

## Payload Structure

### LivesUpdatePayload

**Structure:**
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
| 4 | 1 byte | `uint8_t` | `lives` | New lives count (0-255) |

## Field Specifications

### id

**Type:** `uint32_t` (4 bytes, little-endian)

**Purpose:** Identifies which player's lives changed

**Usage:**
- Must match a player ID in the roster
- Client looks up this player and updates their lives
- If ID not found, client should log warning (may be desync)

### lives

**Type:** `uint8_t` (1 byte)

**Purpose:** New total lives for this player (NOT a delta)

**Range:** 0-255
- 0: Player is dead/out of game
- 1-3: Typical range
- 255: "Infinite" lives mode

**Important:** This is the **absolute new value**, not a change delta.

**Example:**
- Player has 3 lives
- Player is hit
- Server sends `LivesUpdate(id, 2)` ← New total is 2
- NOT: `LivesUpdate(id, -1)` ← We don't send deltas

## Protocol Flow

### Player Loses Life

```
Client                                Server
   │                                     │
   │                  [Player hit]       │
   │                  [Lives: 3 → 2]     │
   │                                     │
   │<──── LivesUpdate (id=1, lives=2) ───────│
   │                                     │
   │ [Update roster locally]             │
   │ [Update HUD]                        │
   │ [Show damage animation]             │
   │                                     │
```

### Player Dies (Lives Reach 0)

```
Client                                Server
   │                                     │
   │                  [Player hit]       │
   │                  [Lives: 1 → 0]     │
   │                                     │
   │<──── LivesUpdate (id=1, lives=0) ───────│
   │                                     │
   │ [Update roster]                     │
   │ [Show death animation]              │
   │ [Display "You died!"]               │
   │                                     │
   │<──── State (player still in world) ──────│
   │                                     │
   │ [Render as ghost/spectator]         │
   │                                     │
```

### Player Respawn (Lives Restored)

```
Client                                Server
   │                                     │
   │                  [Respawn power-up] │
   │                  [Lives: 2 → 3]     │
   │                                     │
   │<──── LivesUpdate (id=1, lives=3) ───────│
   │                                     │
   │ [Update roster]                     │
   │ [Update HUD]                        │
   │ [Play sound effect]                 │
   │                                     │
```

## Server Behavior

### When to Send LivesUpdate

Server sends `LivesUpdate` when:
1. **Player Takes Damage:** Hit by enemy or projectile
2. **Player Dies:** Lives reach 0
3. **Player Respawns:** Lives restored (power-up, checkpoint)
4. **Game Reset:** Lives reset to initial value

### Example Implementation

```cpp
void applyDamageToPlayer(uint32_t player_id) {
    auto it = players_.find(player_id);
    if (it == players_.end()) return;

    auto& player = it->second;

    if (player.lives > 0) {
        player.lives--;

        // Broadcast lives update
        broadcastLivesUpdate(player_id, player.lives);

        if (player.lives == 0) {
            // Player died
            onPlayerDeath(player_id);
        }
    }
}

void broadcastLivesUpdate(uint32_t player_id, uint8_t new_lives) {
    Header header{sizeof(LivesUpdatePayload), MsgType::LivesUpdate, ProtocolVersion};

    LivesUpdatePayload payload;
    payload.id = player_id;
    payload.lives = new_lives;

    uint8_t buffer[sizeof(Header) + sizeof(LivesUpdatePayload)];
    std::memcpy(buffer, &header, sizeof(Header));
    std::memcpy(buffer + sizeof(Header), &payload, sizeof(LivesUpdatePayload));

    // Send to all clients
    for (const auto& [endpoint, pid] : clients_) {
        socket_.send_to(asio::buffer(buffer, sizeof(buffer)), endpoint);
    }
}
```

### Alternative: Send Roster Instead

Server may choose to send full `Roster` message instead of `LivesUpdate`:

**Pros of LivesUpdate:**
- Smaller (9 bytes vs 5 + N×21 bytes)
- More efficient for frequent updates
- Explicit event (easier to trigger animations)

**Pros of Roster:**
- Simpler protocol (fewer message types)
- Always consistent (full state vs delta)
- Includes player names (if changed)

**Recommendation:** Use `LivesUpdate` for frequent updates, send `Roster` occasionally for synchronization.

## Client Behavior

### Receiving and Processing LivesUpdate

**Example Implementation:**
```cpp
void handleLivesUpdate(const uint8_t* payload, size_t payload_size) {
    // Validate size
    if (payload_size != sizeof(LivesUpdatePayload)) {
        return;
    }

    // Parse payload
    LivesUpdatePayload update;
    std::memcpy(&update, payload, sizeof(LivesUpdatePayload));

    // Find player in roster
    auto it = roster_.find(update.id);
    if (it == roster_.end()) {
        // Player not in roster (should not happen)
        std::cerr << "LivesUpdate for unknown player: " << update.id << "\n";
        return;
    }

    // Store old value for comparison
    uint8_t old_lives = it->second.lives;

    // Update lives
    it->second.lives = update.lives;

    // Trigger events
    if (update.lives < old_lives) {
        onPlayerLostLife(update.id, old_lives, update.lives);
    } else if (update.lives > old_lives) {
        onPlayerGainedLife(update.id, old_lives, update.lives);
    }

    // Update UI
    updateScoreboard();

    if (update.id == my_player_id_) {
        updatePlayerHUD();
    }
}

void onPlayerLostLife(uint32_t player_id, uint8_t old_lives, uint8_t new_lives) {
    if (player_id == my_player_id_) {
        // Local player hit
        playSound("player_hit.wav");
        shakeScreen(0.5f);

        if (new_lives == 0) {
            // Died
            playSound("player_death.wav");
            showMessage("You died!");
        }
    } else {
        // Other player hit
        // Optional: Show notification
        showNotification(roster_[player_id].name + " lost a life!");
    }
}

void onPlayerGainedLife(uint32_t player_id, uint8_t old_lives, uint8_t new_lives) {
    if (player_id == my_player_id_) {
        playSound("life_gained.wav");
        showMessage("Extra life!");
    }
}
```

### UI Update

**Example HUD Update:**
```cpp
void updatePlayerHUD() {
    auto it = roster_.find(my_player_id_);
    if (it == roster_.end()) return;

    uint8_t lives = it->second.lives;

    // Draw hearts
    int x = 10;
    int y = screen_height - 40;

    for (uint8_t i = 0; i < lives; ++i) {
        drawIcon(x + i * 30, y, "heart_full");
    }

    // Optional: Draw empty hearts for lost lives
    const uint8_t max_lives = 5;
    for (uint8_t i = lives; i < max_lives; ++i) {
        drawIcon(x + i * 30, y, "heart_empty");
    }
}
```

## Validation

### Server Validation (Before Sending)

**Required:**
- [ ] Player ID exists in game
- [ ] Lives value is reasonable (0-255)

### Client Validation (After Receiving)

**Required:**
- [ ] `header.version == 1`
- [ ] `header.type == 10` (LivesUpdate)
- [ ] `header.size == 5`
- [ ] `payload_size >= 5`

**Optional:**
- [ ] Player ID exists in roster
- [ ] Lives value is reasonable (e.g., ≤ 10)

**Example:**
```cpp
bool validateLivesUpdate(const Header& header, const uint8_t* payload, size_t payload_size) {
    if (header.version != ProtocolVersion) return false;
    if (header.type != MsgType::LivesUpdate) return false;
    if (header.size != sizeof(LivesUpdatePayload)) return false;
    if (payload_size < sizeof(LivesUpdatePayload)) return false;

    // Parse payload
    LivesUpdatePayload update;
    std::memcpy(&update, payload, sizeof(LivesUpdatePayload));

    // Optional: Validate lives value
    if (update.lives > 100) {
        std::cerr << "Suspicious lives value: " << (int)update.lives << "\n";
    }

    return true;
}
```

## Wire Format Examples

### Example 1: Player 1 Has 2 Lives

**Complete Message:**
```
Offset  Hex                      ASCII   Description
------  ----------------------   -----   -----------
0x0000  05 00 0A 01              ....    Header (size=5, type=10, version=1)
0x0004  01 00 00 00              ....    id=1
0x0008  02                       .       lives=2
```

**Total Size:** 9 bytes

### Example 2: Player 42 Died (0 Lives)

**Complete Message:**
```
Offset  Hex                      ASCII   Description
------  ----------------------   -----   -----------
0x0000  05 00 0A 01              ....    Header
0x0004  2A 00 00 00              *...    id=42
0x0008  00                       .       lives=0
```

## Performance Characteristics

### Bandwidth

**Per Update:** 9 bytes

**Scenarios:**
- 1 player hit per 2 seconds: 9 bytes / 2s = 4.5 bytes/s
- 10 players, each hit once per 10 seconds: 90 bytes / 10s = 9 bytes/s

**Comparison:**
- `LivesUpdate`: 9 bytes per event
- `Roster` (4 players): 89 bytes
- **Savings:** 10× smaller for single-player update

**Recommendation:** Use `LivesUpdate` for frequent changes, send `Roster` occasionally for sync.

## Common Issues

### Issue 1: Lives Not Updating in UI

**Problem:** Client receives LivesUpdate but UI doesn't change

**Causes:**
1. Roster not initialized (player ID not in roster)
2. UI update code not called
3. Wrong player ID

**Solution:**
```cpp
void handleLivesUpdate(const LivesUpdatePayload& update) {
    // Ensure roster entry exists
    if (roster_.find(update.id) == roster_.end()) {
        // Create placeholder
        roster_[update.id] = {"Unknown", 0, update.id};
    }

    // Update lives
    roster_[update.id].lives = update.lives;

    // Force UI update
    updateScoreboard();
    updatePlayerHUD();
}
```

### Issue 2: Lives Increase Unexpectedly

**Problem:** Lives go up when they should go down (or vice versa)

**Causes:**
1. Network reordering (old update arrives after new one)
2. Server bug (sending wrong value)

**Solution:**
```cpp
// Client: Add sequence number validation (requires protocol change)
// Or: Ignore updates that would increase lives unexpectedly

void handleLivesUpdate(const LivesUpdatePayload& update) {
    auto it = roster_.find(update.id);
    if (it == roster_.end()) return;

    uint8_t old_lives = it->second.lives;
    uint8_t new_lives = update.lives;

    // Optional: Validate decrease (if game never increases lives)
    if (new_lives > old_lives && old_lives > 0) {
        std::cerr << "Warning: Lives increased unexpectedly\n";
        // May be legitimate (power-up) or network issue
    }

    it->second.lives = new_lives;
}
```

### Issue 3: LivesUpdate for Unknown Player

**Problem:** Receive update for player not in roster

**Causes:**
1. Player joined but Roster not received yet
2. Packet reordering (LivesUpdate before Roster)
3. Player already left (removed from roster)

**Solution:**
```cpp
void handleLivesUpdate(const LivesUpdatePayload& update) {
    auto it = roster_.find(update.id);
    if (it == roster_.end()) {
        // Store pending update
        pending_lives_updates_[update.id] = update.lives;

        // Will be applied when Roster arrives
        return;
    }

    it->second.lives = update.lives;
}

void handleRoster(const std::vector<PlayerEntry>& roster) {
    // Update roster
    // ...

    // Apply pending updates
    for (auto& [id, lives] : pending_lives_updates_) {
        if (roster_.find(id) != roster_.end()) {
            roster_[id].lives = lives;
        }
    }
    pending_lives_updates_.clear();
}
```

## Use Cases

### Damage Indication

```cpp
void onPlayerLostLife(uint32_t player_id, uint8_t old_lives, uint8_t new_lives) {
    // Find player entity
    auto it = entities_.find(player_id);
    if (it == entities_.end()) return;

    // Spawn damage number
    spawnDamageNumber(it->second.x, it->second.y, "- 1 LIFE");

    // Flash red
    it->second.flash_color = Color::Red;
    it->second.flash_timer = 0.5f;

    // Play sound
    playSound("hit.wav");
}
```

### Achievement Tracking

```cpp
void onPlayerLostLife(uint32_t player_id, uint8_t old_lives, uint8_t new_lives) {
    if (new_lives == 0) {
        // Player died
        deaths_this_game_++;

        if (deaths_this_game_ == 0) {
            unlockAchievement("Flawless Victory");
        }
    }
}
```

### Game Over Check

```cpp
void checkGameOver() {
    if (roster_[my_player_id_].lives == 0) {
        // I'm dead
        if (all_players_dead()) {
            showGameOver();
        } else {
            showSpectatorMode();
        }
    }
}

bool all_players_dead() {
    for (const auto& [id, player] : roster_) {
        if (player.lives > 0) {
            return false;
        }
    }
    return true;
}
```

## Testing

### Unit Test Example

```cpp
TEST(Protocol, LivesUpdate_Format) {
    Header header{sizeof(LivesUpdatePayload), MsgType::LivesUpdate, ProtocolVersion};
    LivesUpdatePayload payload{42, 2};

    uint8_t buffer[9];
    std::memcpy(buffer, &header, 4);
    std::memcpy(buffer + 4, &payload, 5);

    // Verify header
    EXPECT_EQ(buffer[0], 0x05);  // size = 5
    EXPECT_EQ(buffer[2], 10);    // type = LivesUpdate

    // Verify payload
    EXPECT_EQ(buffer[4], 0x2A);  // id = 42 (low byte)
    EXPECT_EQ(buffer[8], 2);     // lives = 2
}

TEST(Protocol, LivesUpdate_ClientHandling) {
    Client client;

    // Setup: Player 1 has 3 lives
    client.roster_[1] = {"Alice", 3, 1};

    // Receive LivesUpdate: Player 1 now has 2 lives
    LivesUpdatePayload update{1, 2};
    client.handleLivesUpdate(update);

    // Verify
    EXPECT_EQ(client.roster_[1].lives, 2);
}
```

## Related Documentation

- **[udp-09-roster.md](udp-09-roster.md)** - Roster message (full player list with lives)
- **[udp-11-score-update.md](udp-11-score-update.md)** - ScoreUpdate message (similar pattern)
- **[03-data-structures.md](03-data-structures.md)** - LivesUpdatePayload definition

---

**Last Updated:** 2025-10-29

