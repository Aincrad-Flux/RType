# ScoreUpdate (11) - UDP

## Overview

**Message Type:** `ScoreUpdate` (11)
**Transport:** UDP
**Direction:** Server → Client
**Purpose:** Notify clients of a single player's score change (authoritative)
**Status:** Active
**Frequency:** On event (when any player's score changes)

## Message Format

### Complete Message Structure

```
┌─────────────────────────┬───────────────────────┐
│   Header (4 bytes)      │  Payload (8 bytes)    │
├─────────────────────────┼───────────────────────┤
│ size=8, type=11, ver=1  │  ScoreUpdatePayload   │
└─────────────────────────┴───────────────────────┘
```

**Total Message Size:** 12 bytes

### Header

| Field | Value | Encoding |
|-------|-------|----------|
| `size` | 8 | `uint16_t` (little-endian) |
| `type` | 11 | `uint8_t` (ScoreUpdate) |
| `version` | 1 | `uint8_t` |

## Payload Structure

### ScoreUpdatePayload

**Structure:**
```cpp
#pragma pack(push, 1)
struct ScoreUpdatePayload {
    std::uint32_t id;       // Player entity ID
    std::int32_t score;     // New total score (authoritative)
};
#pragma pack(pop)
```

**Size:** 8 bytes

**Binary Layout:**

| Offset | Size | Type | Field | Description |
|--------|------|------|-------|-------------|
| 0 | 4 bytes | `uint32_t` | `id` | Player entity ID (little-endian) |
| 4 | 4 bytes | `int32_t` | `score` | New score (signed, little-endian) |

## Field Specifications

### id

**Type:** `uint32_t` (4 bytes, little-endian)

**Purpose:** Identifies which player's score changed

**Usage:**
- Must match a player ID in the roster
- Client looks up this player and updates their score
- If ID not found, client should log warning

### score

**Type:** `int32_t` (4 bytes, little-endian)

**Purpose:** New total score for this player (NOT a delta)

**Range:** -2,147,483,648 to 2,147,483,647

**Important:** This is the **absolute new value**, not a change delta.

**Signed Integer:** Can be negative if game has penalties (e.g., friendly fire, death penalty).

**Examples:**
- Player kills enemy: score 1000 → 1100 (server sends 1100)
- Player dies with penalty: score 1100 → 1000 (server sends 1000)
- Bonus collected: score 1000 → 1500 (server sends 1500)

## Protocol Flow

### Player Earns Points

```
Client                                Server
   │                                     │
   │                  [Enemy destroyed]  │
   │                  [Score: 1000 → 1100]│
   │                                     │
   │<──── ScoreUpdate (id=1, score=1100) ────│
   │                                     │
   │ [Update scoreboard]                 │
   │ [Show "+100" popup]                 │
   │ [Play sound]                        │
   │                                     │
```

### Multiple Score Changes

```
Client                                Server
   │                                     │
   │                  [Wave completed]   │
   │                  [Bonus: +500]      │
   │                                     │
   │<──── ScoreUpdate (id=1, score=1600) ────│
   │                                     │
   │                  [Power-up collected]│
   │                  [Bonus: +200]      │
   │                                     │
   │<──── ScoreUpdate (id=1, score=1800) ────│
   │                                     │
```

### Penalty (Negative Delta)

```
Client                                Server
   │                                     │
   │                  [Player death]     │
   │                  [Penalty: -100]    │
   │                  [Score: 1800 → 1700]│
   │                                     │
   │<──── ScoreUpdate (id=1, score=1700) ────│
   │                                     │
   │ [Update scoreboard]                 │
   │ [Show "-100" popup]                 │
   │                                     │
```

## Server Behavior

### When to Send ScoreUpdate

Server sends `ScoreUpdate` when:
1. **Enemy Defeated:** Player earns points for kill
2. **Objective Completed:** Wave clear, boss defeated, etc.
3. **Collectible Obtained:** Power-up, coin, bonus item
4. **Penalty Applied:** Death penalty, friendly fire
5. **Combo/Multiplier:** Score multiplier ends

### Example Implementation

```cpp
void awardPoints(uint32_t player_id, int32_t points) {
    auto it = players_.find(player_id);
    if (it == players_.end()) return;

    // Update score
    it->second.score += points;

    // Broadcast update
    broadcastScoreUpdate(player_id, it->second.score);
}

void broadcastScoreUpdate(uint32_t player_id, int32_t new_score) {
    Header header{sizeof(ScoreUpdatePayload), MsgType::ScoreUpdate, ProtocolVersion};

    ScoreUpdatePayload payload;
    payload.id = player_id;
    payload.score = new_score;

    uint8_t buffer[sizeof(Header) + sizeof(ScoreUpdatePayload)];
    std::memcpy(buffer, &header, sizeof(Header));
    std::memcpy(buffer + sizeof(Header), &payload, sizeof(ScoreUpdatePayload));

    // Send to all clients
    for (const auto& [endpoint, pid] : clients_) {
        socket_.send_to(asio::buffer(buffer, sizeof(buffer)), endpoint);
    }
}

// Score events
void onEnemyKilled(uint32_t killer_id, EnemyType enemy_type) {
    int32_t points = getPointsForEnemy(enemy_type);
    awardPoints(killer_id, points);
}

void onPlayerDeath(uint32_t player_id) {
    const int32_t DEATH_PENALTY = -100;
    awardPoints(player_id, DEATH_PENALTY);
}

void onCollectibleGathered(uint32_t player_id, CollectibleType type) {
    int32_t points = getPointsForCollectible(type);
    awardPoints(player_id, points);
}
```

## Client Behavior

### Receiving and Processing ScoreUpdate

**Example Implementation:**
```cpp
void handleScoreUpdate(const uint8_t* payload, size_t payload_size) {
    // Validate size
    if (payload_size != sizeof(ScoreUpdatePayload)) {
        return;
    }

    // Parse payload
    ScoreUpdatePayload update;
    std::memcpy(&update, payload, sizeof(ScoreUpdatePayload));

    // Find player in score table
    auto it = scores_.find(update.id);
    if (it == scores_.end()) {
        // First score update for this player
        scores_[update.id] = update.score;
        onScoreChanged(update.id, 0, update.score);
        return;
    }

    // Store old score for delta calculation
    int32_t old_score = it->second;
    int32_t new_score = update.score;
    int32_t delta = new_score - old_score;

    // Update score
    it->second = new_score;

    // Trigger events
    onScoreChanged(update.id, old_score, new_score);

    // Update UI
    updateScoreboard();

    if (update.id == my_player_id_) {
        updatePlayerHUD();
        showScorePopup(delta);
    }
}

void onScoreChanged(uint32_t player_id, int32_t old_score, int32_t new_score) {
    int32_t delta = new_score - old_score;

    if (player_id == my_player_id_) {
        // Local player score changed
        if (delta > 0) {
            playSound("score_gain.wav");
            showFloatingText("+" + std::to_string(delta), Color::Green);
        } else if (delta < 0) {
            playSound("score_loss.wav");
            showFloatingText(std::to_string(delta), Color::Red);
        }
    }

    // Check for high score
    if (new_score > high_score_) {
        high_score_ = new_score;
        if (player_id == my_player_id_) {
            showAchievement("New High Score!");
        }
    }
}
```

### UI Update

**Example Scoreboard:**
```cpp
void renderScoreboard() {
    // Sort players by score (descending)
    std::vector<std::pair<uint32_t, int32_t>> sorted_scores;
    for (const auto& [id, score] : scores_) {
        sorted_scores.push_back({id, score});
    }
    std::sort(sorted_scores.begin(), sorted_scores.end(),
        [](const auto& a, const auto& b) {
            return a.second > b.second;
        });

    // Display
    int y = 10;
    int rank = 1;

    drawText(10, y, "RANK  PLAYER          SCORE", Color::White);
    y += 20;

    for (const auto& [id, score] : sorted_scores) {
        Color color = (id == my_player_id_) ? Color::Yellow : Color::White;

        std::string name = roster_[id].name;
        std::string score_str = std::to_string(score);

        drawText(10, y, std::to_string(rank), color);
        drawText(60, y, name, color);
        drawText(200, y, score_str, color);

        y += 20;
        rank++;
    }
}
```

**Example HUD:**
```cpp
void updatePlayerHUD() {
    auto it = scores_.find(my_player_id_);
    if (it == scores_.end()) return;

    int32_t score = it->second;

    // Draw score
    drawText(screen_width - 150, 10, "SCORE", Color::White);
    drawText(screen_width - 150, 30, std::to_string(score), Color::Yellow);
}

void showScorePopup(int32_t delta) {
    // Spawn floating text at player position
    auto it = entities_.find(my_player_id_);
    if (it == entities_.end()) return;

    std::string text = (delta > 0) ? "+" + std::to_string(delta) : std::to_string(delta);
    Color color = (delta > 0) ? Color::Green : Color::Red;

    spawnFloatingText(it->second.x, it->second.y - 30, text, color, 2.0f);
}
```

## Validation

### Server Validation (Before Sending)

**Required:**
- [ ] Player ID exists in game
- [ ] Score value is reasonable

**Optional:**
- [ ] Score doesn't overflow int32 range
- [ ] Score change is reasonable (not jumping by millions)

### Client Validation (After Receiving)

**Required:**
- [ ] `header.version == 1`
- [ ] `header.type == 11` (ScoreUpdate)
- [ ] `header.size == 8`
- [ ] `payload_size >= 8`

**Optional:**
- [ ] Player ID exists in roster
- [ ] Score is reasonable (sanity check)
- [ ] Score delta is reasonable (detect cheating/desync)

**Example:**
```cpp
bool validateScoreUpdate(const Header& header, const ScoreUpdatePayload& update) {
    if (header.version != ProtocolVersion) return false;
    if (header.type != MsgType::ScoreUpdate) return false;
    if (header.size != sizeof(ScoreUpdatePayload)) return false;

    // Optional: Detect suspicious score jumps
    auto it = scores_.find(update.id);
    if (it != scores_.end()) {
        int32_t old_score = it->second;
        int32_t delta = update.score - old_score;

        if (std::abs(delta) > 100000) {
            std::cerr << "Warning: Large score jump: " << delta << "\n";
            // May be legitimate (bonus round) or server error
        }
    }

    return true;
}
```

## Wire Format Examples

### Example 1: Player 1 Has Score 1500

**Complete Message:**
```
Offset  Hex                      ASCII   Description
------  ----------------------   -----   -----------
0x0000  08 00 0B 01              ....    Header (size=8, type=11, version=1)
0x0004  01 00 00 00              ....    id=1
0x0008  DC 05 00 00              ....    score=1500 (0x05DC)
```

**Total Size:** 12 bytes

### Example 2: Player 2 Has Negative Score (-100)

**Complete Message:**
```
Offset  Hex                      ASCII   Description
------  ----------------------   -----   -----------
0x0000  08 00 0B 01              ....    Header
0x0004  02 00 00 00              ....    id=2
0x0008  9C FF FF FF              ....    score=-100 (0xFFFFFF9C, two's complement)
```

### Example 3: Player 42 Has High Score (1,000,000)

**Complete Message:**
```
Offset  Hex                      ASCII   Description
------  ----------------------   -----   -----------
0x0000  08 00 0B 01              ....    Header
0x0004  2A 00 00 00              *...    id=42
0x0008  40 42 0F 00              @B..    score=1,000,000 (0x000F4240)
```

## Performance Characteristics

### Bandwidth

**Per Update:** 12 bytes

**Scenarios:**
- 1 enemy killed per 2 seconds: 12 bytes / 2s = 6 bytes/s
- 10 enemies per second (bullet hell): 120 bytes/s

**Comparison:**
- Sending score in Roster: Requires full Roster broadcast (expensive)
- Sending score in State: Would add 4 bytes per player per frame @ 60 Hz
- `ScoreUpdate`: Only when score changes (efficient)

## Common Issues

### Issue 1: Score Display Not Updating

**Problem:** Client receives ScoreUpdate but UI doesn't change

**Causes:**
1. Score table not initialized
2. UI update code not called
3. Wrong player ID

**Solution:**
```cpp
void handleScoreUpdate(const ScoreUpdatePayload& update) {
    // Update score table (create if doesn't exist)
    scores_[update.id] = update.score;

    // Force UI update
    updateScoreboard();
    updatePlayerHUD();
}
```

### Issue 2: Score Jumps Unexpectedly

**Problem:** Score changes by large amount suddenly

**Causes:**
1. Packet loss (missed intermediate updates)
2. Server sent correct absolute value (not delta)
3. Out-of-order delivery

**Solution:**
```cpp
// Client: Always use absolute value from server
void handleScoreUpdate(const ScoreUpdatePayload& update) {
    // Server is authoritative - trust the score value
    scores_[update.id] = update.score;

    // Optional: Log large jumps for debugging
    if (delta > 10000) {
        std::cout << "Large score change: " << delta << " (may be legitimate)\n";
    }
}
```

### Issue 3: Negative Score Display

**Problem:** Score shows negative value

**Cause:** Intentional (death penalty, friendly fire) or bug

**Solution:**
```cpp
// Display negative scores in red
void renderScore(int32_t score) {
    Color color = (score >= 0) ? Color::Green : Color::Red;
    drawText(10, 10, std::to_string(score), color);
}

// Optional: Prevent negative scores (server-side)
void awardPoints(uint32_t player_id, int32_t delta) {
    players_[player_id].score += delta;

    // Clamp to zero (if game doesn't allow negative)
    if (players_[player_id].score < 0) {
        players_[player_id].score = 0;
    }

    broadcastScoreUpdate(player_id, players_[player_id].score);
}
```

## Use Cases

### Leaderboard

```cpp
void renderLeaderboard() {
    // Sort by score
    std::vector<std::pair<std::string, int32_t>> leaderboard;
    for (const auto& [id, score] : scores_) {
        leaderboard.push_back({roster_[id].name, score});
    }
    std::sort(leaderboard.begin(), leaderboard.end(),
        [](const auto& a, const auto& b) { return a.second > b.second; });

    // Display top 10
    for (size_t i = 0; i < std::min(leaderboard.size(), size_t(10)); ++i) {
        drawText(10, 10 + i * 20,
                 std::to_string(i + 1) + ". " + leaderboard[i].first + " - " +
                 std::to_string(leaderboard[i].second));
    }
}
```

### Achievement Tracking

```cpp
void onScoreChanged(uint32_t player_id, int32_t old_score, int32_t new_score) {
    if (player_id != my_player_id_) return;

    // Check achievement thresholds
    if (old_score < 10000 && new_score >= 10000) {
        unlockAchievement("Rookie Scorer");
    }
    if (old_score < 100000 && new_score >= 100000) {
        unlockAchievement("Master Scorer");
    }
}
```

### Combo System

```cpp
void onScoreChanged(uint32_t player_id, int32_t old_score, int32_t new_score) {
    if (player_id != my_player_id_) return;

    int32_t delta = new_score - old_score;

    if (delta > 0) {
        // Score gained, increase combo
        combo_timer_ = 3.0f;  // Reset timer
        combo_count_++;

        if (combo_count_ >= 10) {
            showMessage("COMBO x" + std::to_string(combo_count_) + "!");
        }
    }
}

void update(float dt) {
    if (combo_timer_ > 0) {
        combo_timer_ -= dt;
        if (combo_timer_ <= 0) {
            // Combo expired
            combo_count_ = 0;
        }
    }
}
```

## Testing

### Unit Test Example

```cpp
TEST(Protocol, ScoreUpdate_Format) {
    Header header{sizeof(ScoreUpdatePayload), MsgType::ScoreUpdate, ProtocolVersion};
    ScoreUpdatePayload payload{1, 1500};

    uint8_t buffer[12];
    std::memcpy(buffer, &header, 4);
    std::memcpy(buffer + 4, &payload, 8);

    // Verify header
    EXPECT_EQ(buffer[0], 0x08);  // size = 8
    EXPECT_EQ(buffer[2], 11);    // type = ScoreUpdate

    // Verify payload
    EXPECT_EQ(buffer[4], 0x01);  // id = 1
    EXPECT_EQ(buffer[8], 0xDC);  // score = 1500 (low byte)
}

TEST(Protocol, ScoreUpdate_Negative) {
    ScoreUpdatePayload payload{2, -100};

    // Verify negative value encoding
    int32_t score;
    std::memcpy(&score, &payload.score, 4);
    EXPECT_EQ(score, -100);
}
```

## Related Documentation

- **[udp-09-roster.md](udp-09-roster.md)** - Roster message (player list)
- **[udp-10-lives-update.md](udp-10-lives-update.md)** - LivesUpdate message (similar pattern)
- **[03-data-structures.md](03-data-structures.md)** - ScoreUpdatePayload definition

---

**Last Updated:** 2025-10-29

