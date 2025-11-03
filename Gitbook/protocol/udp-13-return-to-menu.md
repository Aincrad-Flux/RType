# ReturnToMenu (13) - UDP

## Overview

**Message Type:** `ReturnToMenu` (13)
**Transport:** UDP
**Direction:** Server → Client
**Purpose:** Request client to return to menu (game ending, insufficient players)
**Status:** Active
**Frequency:** On event (game end condition met)

## Message Format

### Complete Message Structure

```
┌─────────────────────────┐
│   Header (4 bytes)      │
│  (No Payload)           │
├─────────────────────────┤
│ size=0, type=13, ver=1  │
└─────────────────────────┘
```

**Total Message Size:** 4 bytes (header only)

### Header

| Field | Value | Encoding |
|-------|-------|----------|
| `size` | 0 | `uint16_t` (little-endian) |
| `type` | 13 | `uint8_t` (ReturnToMenu) |
| `version` | 1 | `uint8_t` |

**Wire Format:**
```
00 00 0D 01
└─ size=0 (no payload)
      └─ type=13 (ReturnToMenu)
         └─ version=1
```

## Payload

**No Payload** - This message consists of the header only.

**Note:** Future versions could add a payload to indicate the reason for returning to menu (e.g., `reason_code: uint8_t`).

## Protocol Flow

### Insufficient Players

```
Client A                             Server                              Client B
   │                                    │                                    │
   │                                    │<──── Disconnect ───────────────────│
   │                                    │                                    │
   │                [Player B left]     │                                    │
   │                [Only 1 player remains]│                                    │
   │                [Game requires 2+]  │                                    │
   │                                    │                                    │
   │<──── ReturnToMenu ─────────────────────│                                    │
   │                                    │                                    │
   │ [Show message: "Not enough players"]│                                    │
   │ [Return to lobby/menu]             │                                    │
   │                                    │                                    │
   │───── Disconnect ──────────────────────>│                                    │
   │                                    │                                    │
```

### Game Completed

```
Client                                Server
   │                                     │
   │                [All enemies defeated]│
   │                [Wave complete]      │
   │                                     │
   │<──── ReturnToMenu ────────────────────│
   │                                     │
   │ [Show "Victory!" screen]            │
   │ [Display final scores]              │
   │ [Wait for user input]               │
   │                                     │
   │───── Disconnect ──────────────────────>│
   │                                     │
   │ [Return to main menu]               │
   │                                     │
```

### Server Shutdown

```
Client                                Server
   │                                     │
   │                  [Admin shutdown]   │
   │                                     │
   │<──── ReturnToMenu (all clients) ─────│
   │                                     │
   │ [Show "Server closing"]             │
   │ [Graceful disconnect]               │
   │                                     │
```

## Server Behavior

### When to Send ReturnToMenu

Server sends `ReturnToMenu` when:
1. **Insufficient Players:** Player count drops below minimum (e.g., < 2)
2. **Game Complete:** All objectives completed, game won
3. **Game Over:** All players dead
4. **Server Shutdown:** Admin closes server
5. **Timeout:** No activity for extended period
6. **Error:** Critical server error prevents continuation

### Example Implementation

```cpp
void checkGameEndConditions() {
    // Check player count
    if (players_.size() < MIN_PLAYERS) {
        broadcastReturnToMenu();
        return;
    }

    // Check game over (all players dead)
    if (allPlayersDead()) {
        broadcastReturnToMenu();
        return;
    }

    // Check victory condition
    if (allWavesCompleted()) {
        broadcastReturnToMenu();
        return;
    }
}

void broadcastReturnToMenu() {
    Header header;
    header.size = 0;
    header.type = MsgType::ReturnToMenu;
    header.version = ProtocolVersion;

    // Send to all clients
    for (const auto& [endpoint, player_id] : clients_) {
        try {
            socket_.send_to(asio::buffer(&header, sizeof(Header)), endpoint);
        } catch (const std::exception& e) {
            std::cerr << "Failed to send ReturnToMenu to " << endpoint << ": "
                      << e.what() << "\n";
        }
    }

    // Log event
    std::cout << "Sent ReturnToMenu to " << clients_.size() << " clients\n";

    // Optional: Prepare for reset or shutdown
    prepareForReset();
}

void prepareForReset() {
    // Clear game state
    entities_.clear();
    players_.clear();

    // But keep client connections if restarting
    // Or close everything if shutting down
}
```

### Checking Conditions

**Minimum Players:**
```cpp
void onPlayerDisconnect(uint32_t player_id) {
    removePlayer(player_id);

    // Check if we have enough players
    if (players_.size() < MIN_PLAYERS) {
        std::cout << "Not enough players, ending game\n";
        broadcastReturnToMenu();
    }
}
```

**All Players Dead:**
```cpp
bool allPlayersDead() const {
    for (const auto& [id, player] : players_) {
        if (player.lives > 0) {
            return false;
        }
    }
    return true;  // All dead (or no players)
}

void checkGameOver() {
    if (allPlayersDead() && !players_.empty()) {
        std::cout << "Game over: all players dead\n";
        broadcastReturnToMenu();
    }
}
```

**Victory Condition:**
```cpp
bool allWavesCompleted() const {
    return current_wave_ >= MAX_WAVES && enemies_.empty();
}

void tick() {
    // ... game logic

    if (allWavesCompleted()) {
        std::cout << "Victory! All waves completed\n";
        broadcastReturnToMenu();
    }
}
```

## Client Behavior

### On Receiving ReturnToMenu

**Example Implementation:**
```cpp
void handleReturnToMenu() {
    std::cout << "Server requested return to menu\n";

    // Determine reason (if no payload, must infer)
    std::string reason = inferReason();

    // Show end game screen
    showEndGameScreen(reason);

    // Wait for user acknowledgment (or auto-return after timeout)
    waitForUserAcknowledgment(5.0f);  // 5 seconds

    // Disconnect gracefully
    sendDisconnect();

    // Return to menu
    returnToMainMenu();
}

std::string inferReason() {
    // Try to infer reason from game state
    if (roster_.size() < 2) {
        return "Not enough players";
    }

    bool all_dead = true;
    for (const auto& [id, player] : roster_) {
        if (player.lives > 0) {
            all_dead = false;
            break;
        }
    }

    if (all_dead) {
        return "Game Over - All players defeated";
    }

    // Default
    return "Game ended";
}

void showEndGameScreen(const std::string& reason) {
    // Display message
    showMessage(reason);

    // Show final scores
    renderFinalScoreboard();

    // Show stats (optional)
    displayGameStats();

    // Play end game music/sound
    playSound("game_end.wav");
}

void returnToMainMenu() {
    // Clean up game state
    entities_.clear();
    roster_.clear();
    scores_.clear();

    // Stop game loop
    game_running_ = false;

    // Load menu
    showMainMenu();

    // Reset connection state
    connected_ = false;
    my_player_id_ = 0;
}
```

### User Acknowledgment

**With Timeout:**
```cpp
void waitForUserAcknowledgment(float timeout_seconds) {
    auto start = std::chrono::steady_clock::now();
    bool acknowledged = false;

    while (!acknowledged) {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration<float>(now - start).count();

        if (elapsed > timeout_seconds) {
            // Timeout, proceed anyway
            break;
        }

        // Check for user input (any key, click, etc.)
        if (isAnyInputPressed()) {
            acknowledged = true;
            break;
        }

        // Render end screen
        renderEndGameScreen(timeout_seconds - elapsed);

        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }
}

void renderEndGameScreen(float time_remaining) {
    // Show message
    drawText(screen_width / 2, 100, "Game Ended", Color::Yellow);
    drawText(screen_width / 2, 150, reason_, Color::White);

    // Show scoreboard
    renderFinalScoreboard();

    // Show countdown or prompt
    if (time_remaining > 0) {
        drawText(screen_width / 2, screen_height - 50,
                 "Returning to menu in " + std::to_string((int)time_remaining) + "s",
                 Color::Gray);
        drawText(screen_width / 2, screen_height - 30,
                 "Press any key to continue",
                 Color::Gray);
    }
}
```

## Validation

### Server Validation (Before Sending)

**Required:**
- [ ] Game end condition is met
- [ ] Clients list is not empty (someone to send to)

### Client Validation (After Receiving)

**Required:**
- [ ] `header.version == 1`
- [ ] `header.type == 13` (ReturnToMenu)
- [ ] `header.size == 0`

**Example:**
```cpp
bool validateReturnToMenu(const Header& header) {
    if (header.version != ProtocolVersion) {
        std::cerr << "Wrong protocol version\n";
        return false;
    }

    if (header.type != MsgType::ReturnToMenu) {
        return false;
    }

    if (header.size != 0) {
        std::cerr << "ReturnToMenu has unexpected payload\n";
        // Still valid, just unexpected
    }

    return true;
}
```

## Wire Format Example

### ReturnToMenu Message

**Complete Message:**
```
Offset  Hex              ASCII   Description
------  --------------   -----   -----------
0x0000  00 00 0D 01      ....    Header (size=0, type=13, version=1)
```

**Total Size:** 4 bytes

**Breakdown:**
- Byte 0: `0x00` - size low byte (0)
- Byte 1: `0x00` - size high byte (0)
- Byte 2: `0x0D` - type (13 = ReturnToMenu)
- Byte 3: `0x01` - version (1)

## Common Issues

### Issue 1: Client Doesn't Return to Menu

**Problem:** Client receives ReturnToMenu but stays in game

**Causes:**
1. Message not handled
2. Client ignores message
3. UI state machine bug

**Solution:**
```cpp
// Ensure ReturnToMenu is handled
void handlePacket(const uint8_t* buffer, size_t length) {
    // ... parse header

    switch (header.type) {
        case MsgType::ReturnToMenu:
            handleReturnToMenu();
            break;
        // ... other cases
    }
}

// Implement handling
void handleReturnToMenu() {
    if (!game_running_) {
        return;  // Already in menu
    }

    // Force return to menu
    game_running_ = false;
    showEndGameScreen();
}
```

### Issue 2: ReturnToMenu Sent Too Early

**Problem:** Server sends ReturnToMenu prematurely

**Cause:** Condition check is incorrect

**Solution:**
```cpp
// Add grace period before ending game
void onPlayerDisconnect(uint32_t player_id) {
    removePlayer(player_id);

    if (players_.size() < MIN_PLAYERS) {
        // Start grace period (30 seconds to rejoin)
        grace_period_start_ = std::chrono::steady_clock::now();
        grace_period_active_ = true;
    }
}

void tick() {
    if (grace_period_active_) {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = now - grace_period_start_;

        if (players_.size() >= MIN_PLAYERS) {
            // Player rejoined, cancel grace period
            grace_period_active_ = false;
        } else if (elapsed > std::chrono::seconds(30)) {
            // Grace period expired, end game
            broadcastReturnToMenu();
            grace_period_active_ = false;
        }
    }
}
```

### Issue 3: Client Stuck on End Screen

**Problem:** Client never returns to menu after ReturnToMenu

**Cause:** Waiting for user input indefinitely

**Solution:**
```cpp
// Always have timeout
void showEndGameScreen() {
    const float TIMEOUT = 30.0f;  // 30 seconds max
    auto start = std::chrono::steady_clock::now();

    while (true) {
        auto now = std::chrono::steady_clock::now();
        float elapsed = std::chrono::duration<float>(now - start).count();

        if (elapsed > TIMEOUT || isAnyInputPressed()) {
            break;
        }

        render();
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }

    // Proceed to menu
    returnToMainMenu();
}
```

## Use Cases

### Lobby System

```cpp
// Server: End game when players leave
void onPlayerCount Changed() {
    if (game_state_ == GameState::Playing && players_.size() < MIN_PLAYERS) {
        broadcastReturnToMenu();
        game_state_ = GameState::Lobby;
    }
}
```

### Campaign Mode

```cpp
// Server: End level when complete
void onLevelComplete() {
    broadcastReturnToMenu();

    // Server prepares next level
    current_level_++;
    if (current_level_ < MAX_LEVELS) {
        loadLevel(current_level_);
        // Clients will rejoin for next level
    }
}
```

### Tournament Mode

```cpp
// Server: End match after time limit
void checkTimeLimit() {
    if (match_time_ >= MATCH_DURATION) {
        // Time's up, determine winner
        determineWinner();
        broadcastReturnToMenu();
    }
}
```

## Future Enhancements

### Payload with Reason Code

**Proposed Enhancement:**
```cpp
#pragma pack(push, 1)
struct ReturnToMenuPayload {
    uint8_t reason;  // 0=unknown, 1=not enough players, 2=game over, 3=victory, 4=server shutdown
};
#pragma pack(pop)
```

**Benefits:**
- Client can show specific message
- Client can handle differently (e.g., victory vs defeat)
- Better user experience

**Implementation:**
```cpp
enum class ReturnReason : uint8_t {
    Unknown = 0,
    InsufficientPlayers = 1,
    GameOver = 2,
    Victory = 3,
    ServerShutdown = 4,
    Timeout = 5,
};

void broadcastReturnToMenu(ReturnReason reason) {
    // ... send with payload
}
```

## Testing

### Unit Test Example

```cpp
TEST(Protocol, ReturnToMenu_Format) {
    Header header;
    header.size = 0;
    header.type = MsgType::ReturnToMenu;
    header.version = ProtocolVersion;

    uint8_t buffer[4];
    std::memcpy(buffer, &header, 4);

    // Verify wire format
    EXPECT_EQ(buffer[0], 0x00);  // size low
    EXPECT_EQ(buffer[1], 0x00);  // size high
    EXPECT_EQ(buffer[2], 13);    // type
    EXPECT_EQ(buffer[3], 1);     // version
}

TEST(Protocol, ReturnToMenu_ClientHandling) {
    Client client;
    client.connect();
    client.startGame();

    ASSERT_TRUE(client.isInGame());

    // Receive ReturnToMenu
    client.handleReturnToMenu();

    // Verify client returned to menu
    EXPECT_FALSE(client.isInGame());
    EXPECT_TRUE(client.isInMenu());
}
```

### Integration Test

```cpp
TEST(Protocol, InsufficientPlayers) {
    Server server;
    Client client1, client2;

    // Both clients connect
    client1.connect(server);
    client2.connect(server);

    server.startGame();

    // Client 2 disconnects
    client2.disconnect();
    server.processMessages();

    // Server should send ReturnToMenu to client 1
    EXPECT_TRUE(server.sentReturnToMenu());

    // Client 1 receives message
    client1.processMessages();

    // Client 1 should be in menu
    EXPECT_FALSE(client1.isInGame());
}
```

## Related Documentation

- **[udp-12-disconnect.md](udp-12-disconnect.md)** - Disconnect message (client response to ReturnToMenu)
- **[udp-09-roster.md](udp-09-roster.md)** - Roster message (player count monitoring)
- **[00-overview.md](00-overview.md)** - Protocol overview

---

**Last Updated:** 2025-10-29

