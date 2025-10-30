# Roster (9) - UDP

## Overview

**Message Type:** `Roster` (9)
**Transport:** UDP
**Direction:** Server → Client
**Purpose:** Broadcast list of all players with names and lives
**Status:** Active
**Frequency:** On change (player join, leave, or lives update)

## Message Format

### Complete Message Structure

```
┌─────────────────────────┬────────────────────────────────────┐
│   Header (4 bytes)      │  Payload (1 + N×21 bytes)          │
├─────────────────────────┼────────────────────────────────────┤
│ size=?, type=9, ver=1   │  RosterHeader + N × PlayerEntry    │
└─────────────────────────┴────────────────────────────────────┘
```

**Total Message Size:** 5 + (N × 21) bytes
- Header: 4 bytes
- RosterHeader: 1 byte
- PlayerEntry: 21 bytes each
- **N:** Number of players (0-255)

### Header

| Field | Value | Encoding |
|-------|-------|----------|
| `size` | 1 + (N × 21) | `uint16_t` (little-endian) |
| `type` | 9 | `uint8_t` (Roster) |
| `version` | 1 | `uint8_t` |

## Data Structures

### RosterHeader

**Structure:**
```cpp
#pragma pack(push, 1)
struct RosterHeader {
    std::uint8_t count;  // Number of PlayerEntry records following
};
#pragma pack(pop)
```

**Size:** 1 byte

**Field:**
- `count`: Number of players (0-255)

### PlayerEntry

**Structure:**
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
| 4 | 1 byte | `uint8_t` | `lives` | Lives remaining (0-255) |
| 5 | 16 bytes | `char[16]` | `name` | Player name (null-terminated UTF-8) |

## Field Specifications

### count (RosterHeader)

**Type:** `uint8_t` (1 byte)

**Purpose:** Number of PlayerEntry structures that follow

**Range:** 0-255 players

**Usage:**
- 0: Empty game (no players)
- 1-8: Typical game size
- 255: Maximum players

### id (PlayerEntry)

**Type:** `uint32_t` (4 bytes, little-endian)

**Purpose:** Unique player identifier, matches entity ID in State messages

**Usage:**
- Client uses to associate roster info with entities in State
- Client uses to identify which player in roster is the local player

**Example:**
```cpp
void handleRoster(const std::vector<PlayerEntry>& roster) {
    for (const auto& player : roster) {
        if (player.id == my_player_id_) {
            // This is me!
            displaySelfInfo(player);
        } else {
            displayOtherPlayerInfo(player);
        }
    }
}
```

### lives (PlayerEntry)

**Type:** `uint8_t` (1 byte)

**Purpose:** Number of lives remaining for this player

**Range:** 0-255
- 0: Dead/spectating
- 1-3: Typical range
- 255: "Infinite" lives (practice mode)

**Usage:**
- Display in scoreboard
- Determine game over condition
- Show life indicators in UI

### name (PlayerEntry)

**Type:** `char[16]` (16 bytes, null-terminated UTF-8)

**Purpose:** Player's display name

**Constraints:**
- Maximum 15 displayable characters + null terminator
- Null-terminated within 16 bytes
- Unused bytes zero-padded
- UTF-8 encoded

**Example:**
```cpp
// Extract name as std::string
std::string getPlayerName(const PlayerEntry& player) {
    // Ensure null termination
    char name_buf[17];
    std::memcpy(name_buf, player.name, 16);
    name_buf[16] = '\0';

    return std::string(name_buf);
}
```

## Protocol Flow

### Initial Roster (Player Joins)

```
Client A                             Server                              Client B
   │                                    │                                    │
   │───── UDP Hello ──────────────────────>│                                    │
   │                                    │                                    │
   │                 [Create player A]  │                                    │
   │                                    │                                    │
   │<──── Roster (A) ──────────────────────│                                    │
   │                                    │                                    │
   │                                    │                                    │
   │                                    │<──── UDP Hello ───────────────────────│
   │                                    │                                    │
   │                 [Create player B]  │                                    │
   │                                    │                                    │
   │<──── Roster (A, B) ────────────────────│                                    │
   │                                    │────── Roster (A, B) ──────────────────>│
   │                                    │                                    │
```

### Lives Update (Player Loses Life)

```
Client                                Server
   │                                     │
   │                   [Player hit]      │
   │                   [Lives: 3 → 2]    │
   │                                     │
   │<──── LivesUpdate (id, lives=2) ────────│  Immediate event
   │                                     │
   │<──── Roster (updated) ─────────────────│  Optional: Full roster
   │                                     │
```

### Player Leaves

```
Client A                             Server                              Client B
   │                                    │                                    │
   │───── Disconnect ─────────────────────>│                                    │
   │                                    │                                    │
   │                 [Remove player A]  │                                    │
   │                                    │                                    │
   │                                    │────── Roster (B only) ────────────────>│
   │                                    │                                    │
```

## Server Behavior

### When to Send Roster

Server sends Roster when:
1. **Player Joins:** After processing UDP Hello
2. **Player Leaves:** After disconnect or timeout
3. **Lives Change:** After LivesUpdate (optional, may send LivesUpdate only)
4. **Periodically:** Every N seconds for synchronization (optional)

### Building Roster Message

**Example Implementation:**
```cpp
void broadcastRoster() {
    // Count active players
    size_t player_count = players_.size();
    if (player_count > 255) {
        player_count = 255;  // Limit
    }

    // Calculate sizes
    size_t payload_size = sizeof(RosterHeader) + player_count * sizeof(PlayerEntry);

    // Allocate buffer
    std::vector<uint8_t> buffer(sizeof(Header) + payload_size);

    // Write header
    Header* header = reinterpret_cast<Header*>(buffer.data());
    header->size = payload_size;
    header->type = MsgType::Roster;
    header->version = ProtocolVersion;

    // Write roster header
    RosterHeader* roster_hdr = reinterpret_cast<RosterHeader*>(
        buffer.data() + sizeof(Header)
    );
    roster_hdr->count = player_count;

    // Write player entries
    PlayerEntry* entries = reinterpret_cast<PlayerEntry*>(
        buffer.data() + sizeof(Header) + sizeof(RosterHeader)
    );

    size_t idx = 0;
    for (const auto& [id, player] : players_) {
        if (idx >= player_count) break;

        entries[idx].id = id;
        entries[idx].lives = player.lives;

        // Copy name (truncate if needed)
        size_t name_len = std::min(player.name.size(), size_t(15));
        std::memcpy(entries[idx].name, player.name.c_str(), name_len);
        entries[idx].name[name_len] = '\0';

        // Zero-fill remaining bytes
        std::memset(entries[idx].name + name_len + 1, 0, 15 - name_len);

        ++idx;
    }

    // Broadcast to all clients
    for (const auto& [endpoint, player_id] : clients_) {
        socket_.send_to(asio::buffer(buffer), endpoint);
    }
}
```

## Client Behavior

### Receiving and Parsing Roster

**Example Implementation:**
```cpp
void handleRoster(const uint8_t* payload, size_t payload_size) {
    // Validate minimum size
    if (payload_size < sizeof(RosterHeader)) {
        return;
    }

    // Parse roster header
    RosterHeader roster_hdr;
    std::memcpy(&roster_hdr, payload, sizeof(RosterHeader));

    // Validate size
    size_t expected_size = sizeof(RosterHeader) + roster_hdr.count * sizeof(PlayerEntry);
    if (payload_size < expected_size) {
        return;  // Truncated
    }

    // Parse player entries
    const uint8_t* entries_data = payload + sizeof(RosterHeader);

    // Clear current roster
    roster_.clear();

    for (uint8_t i = 0; i < roster_hdr.count; ++i) {
        PlayerEntry entry;
        std::memcpy(&entry, entries_data + i * sizeof(PlayerEntry), sizeof(PlayerEntry));

        // Ensure name is null-terminated
        entry.name[15] = '\0';

        // Store player info
        PlayerInfo info;
        info.id = entry.id;
        info.lives = entry.lives;
        info.name = std::string(entry.name);

        roster_[entry.id] = info;
    }

    // Update UI
    updateScoreboard();
}
```

### Displaying Roster

**Example UI:**
```cpp
void renderScoreboard() {
    int y = 10;

    drawText(10, y, "PLAYERS", Color::White);
    y += 20;

    for (const auto& [id, player] : roster_) {
        Color color = (id == my_player_id_) ? Color::Yellow : Color::White;

        // Draw name
        drawText(10, y, player.name, color);

        // Draw lives (hearts)
        for (int i = 0; i < player.lives; ++i) {
            drawIcon(100 + i * 20, y, "heart");
        }

        // Highlight if dead
        if (player.lives == 0) {
            drawText(200, y, "[DEAD]", Color::Red);
        }

        y += 20;
    }
}
```

## Validation

### Server Validation (Before Sending)

**Required:**
- [ ] Player count ≤ 255
- [ ] All player IDs are valid
- [ ] All names are null-terminated
- [ ] Lives values are reasonable

### Client Validation (After Receiving)

**Required:**
- [ ] `header.version == 1`
- [ ] `header.type == 9` (Roster)
- [ ] `payload_size >= 1`
- [ ] `payload_size >= 1 + (count × 21)`

**Optional:**
- [ ] Validate player names (UTF-8, printable characters)
- [ ] Validate lives are reasonable (e.g., ≤ 10)

**Example:**
```cpp
bool validateRoster(const Header& header, const uint8_t* payload, size_t payload_size) {
    if (header.version != ProtocolVersion) return false;
    if (header.type != MsgType::Roster) return false;
    if (payload_size < sizeof(RosterHeader)) return false;

    RosterHeader roster_hdr;
    std::memcpy(&roster_hdr, payload, sizeof(RosterHeader));

    size_t expected_size = sizeof(RosterHeader) + roster_hdr.count * sizeof(PlayerEntry);
    if (payload_size < expected_size) {
        std::cerr << "Roster size mismatch\n";
        return false;
    }

    return true;
}
```

## Wire Format Examples

### Example 1: Empty Roster (No Players)

**Complete Message:**
```
Offset  Hex              ASCII   Description
------  --------------   -----   -----------
0x0000  01 00 09 01      ....    Header (size=1, type=9, version=1)
0x0004  00               .       RosterHeader (count=0)
```

**Total Size:** 5 bytes

### Example 2: Single Player

**Player:** ID=1, Lives=3, Name="Alice"

**Complete Message:**
```
Offset  Hex                                              Description
------  ----------------------------------------------   -----------
0x0000  16 00 09 01                                      Header (size=22, type=9, version=1)
0x0004  01                                               RosterHeader (count=1)
0x0005  01 00 00 00                                      id=1
0x0009  03                                               lives=3
0x000A  41 6C 69 63 65 00 00 00 00 00 00 00 00 00 00 00  name="Alice\0" (zero-padded)
```

**Total Size:** 26 bytes

### Example 3: Two Players

**Player 1:** ID=1, Lives=3, Name="Alice"
**Player 2:** ID=2, Lives=2, Name="Bob"

**Complete Message:**
```
Offset  Hex                                              Description
------  ----------------------------------------------   -----------
0x0000  2B 00 09 01                                      Header (size=43, type=9)
0x0004  02                                               RosterHeader (count=2)
0x0005  01 00 00 00                                      Player 1: id=1
0x0009  03                                               Player 1: lives=3
0x000A  41 6C 69 63 65 00 ...                            Player 1: name="Alice"
0x001A  02 00 00 00                                      Player 2: id=2
0x001E  02                                               Player 2: lives=2
0x001F  42 6F 62 00 ...                                  Player 2: name="Bob"
```

**Total Size:** 47 bytes

## Performance Characteristics

### Bandwidth

**Per Player:** 21 bytes

**Message Size Examples:**
| Players | Payload | Total | @ 1 Hz | @ 0.1 Hz |
|---------|---------|-------|--------|----------|
| 0 | 1 | 5 | 40 bps | 4 bps |
| 2 | 43 | 47 | 376 bps | 38 bps |
| 4 | 85 | 89 | 712 bps | 71 bps |
| 8 | 169 | 173 | 1.4 Kbps | 138 bps |
| 16 | 337 | 341 | 2.7 Kbps | 273 bps |

**Recommendations:**
- Send Roster only on changes (not periodically)
- For frequent lives updates, use `LivesUpdate` instead of full Roster
- Roster bandwidth is minimal compared to State messages

## Common Issues

### Issue 1: Name Truncation

**Problem:** Long player names are cut off

**Cause:** Name exceeds 15 characters

**Solution:**
```cpp
// Server: Truncate name when creating PlayerEntry
void truncateName(char* dest, const std::string& src) {
    size_t len = std::min(src.size(), size_t(15));
    std::memcpy(dest, src.c_str(), len);
    dest[len] = '\0';
    std::memset(dest + len + 1, 0, 15 - len);
}

// Client: Handle truncated names gracefully
std::string name = roster[id].name;  // Already null-terminated
if (name.size() == 15) {
    name += "...";  // Indicate truncation in UI
}
```

### Issue 2: Player Not in Roster

**Problem:** Client's local player ID not found in Roster

**Causes:**
1. Roster arrived before player was created server-side
2. Player was removed (disconnect) but client didn't handle it

**Solution:**
```cpp
void handleRoster(const std::vector<PlayerEntry>& roster) {
    bool found_self = false;

    for (const auto& player : roster) {
        if (player.id == my_player_id_) {
            found_self = true;
            break;
        }
    }

    if (!found_self && my_player_id_ != 0) {
        // I'm not in the roster!
        // Possible reasons: kicked, timeout, server error
        handleDisconnect();
    }
}
```

### Issue 3: Roster Out of Sync

**Problem:** Roster shows different players than State message

**Causes:**
1. Packet loss (Roster lost, but State arrived)
2. Out-of-order delivery
3. Server inconsistency (roster not updated after State change)

**Solution:**
```cpp
// Reconcile Roster with State
void reconcileRosterWithState() {
    // Players in State but not in Roster?
    for (const auto& [id, entity] : entities_) {
        if (entity.type == EntityType::Player && roster_.find(id) == roster_.end()) {
            // Player in game but not in roster
            // Create placeholder roster entry
            roster_[id] = {"Unknown", 3, id};
        }
    }
}
```

## Use Cases

### Scoreboard Display

```cpp
void renderScoreboard() {
    // Sort players by lives (descending)
    std::vector<PlayerInfo> sorted_roster;
    for (const auto& [id, player] : roster_) {
        sorted_roster.push_back(player);
    }
    std::sort(sorted_roster.begin(), sorted_roster.end(),
        [](const PlayerInfo& a, const PlayerInfo& b) {
            return a.lives > b.lives;
        });

    // Display
    int y = 10;
    for (const auto& player : sorted_roster) {
        drawText(10, y, player.name);
        drawText(200, y, "Lives: " + std::to_string(player.lives));
        y += 20;
    }
}
```

### Game Over Detection

```cpp
void checkGameOver() {
    int alive_players = 0;

    for (const auto& [id, player] : roster_) {
        if (player.lives > 0) {
            alive_players++;
        }
    }

    if (alive_players == 0) {
        // Game over: all players dead
        showGameOver();
    } else if (alive_players == 1) {
        // Last player standing
        showLastManStanding();
    }
}
```

## Testing

### Unit Test Example

```cpp
TEST(Protocol, Roster_Serialization) {
    RosterHeader roster_hdr{2};
    PlayerEntry players[2] = {
        {1, 3, "Alice"},
        {2, 2, "Bob"}
    };

    // Ensure names are zero-padded
    std::memset(players[0].name + 6, 0, 10);
    std::memset(players[1].name + 4, 0, 12);

    size_t payload_size = sizeof(RosterHeader) + 2 * sizeof(PlayerEntry);
    Header header{(uint16_t)payload_size, MsgType::Roster, ProtocolVersion};

    // Serialize
    std::vector<uint8_t> buffer(sizeof(Header) + payload_size);
    // ... (serialize header, roster_hdr, players)

    // Verify
    EXPECT_EQ(buffer[2], 9);  // type = Roster
    EXPECT_EQ(buffer[4], 2);  // count = 2
}
```

## Related Documentation

- **[udp-10-lives-update.md](udp-10-lives-update.md)** - LivesUpdate message (alternative to full Roster)
- **[udp-11-score-update.md](udp-11-score-update.md)** - ScoreUpdate message
- **[03-data-structures.md](03-data-structures.md)** - PlayerEntry, RosterHeader definitions

---

**Last Updated:** 2025-10-29

