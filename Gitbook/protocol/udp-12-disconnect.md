# Disconnect (12) - UDP

## Overview

**Message Type:** `Disconnect` (12)
**Transport:** UDP
**Direction:** Client → Server
**Purpose:** Explicit notification that client is disconnecting gracefully
**Status:** Active
**Frequency:** Once at disconnect (optional)

## Message Format

### Complete Message Structure

```
┌─────────────────────────┐
│   Header (4 bytes)      │
│  (No Payload)           │
├─────────────────────────┤
│ size=0, type=12, ver=1  │
└─────────────────────────┘
```

**Total Message Size:** 4 bytes (header only)

### Header

| Field | Value | Encoding |
|-------|-------|----------|
| `size` | 0 | `uint16_t` (little-endian) |
| `type` | 12 | `uint8_t` (Disconnect) |
| `version` | 1 | `uint8_t` |

**Wire Format:**
```
00 00 0C 01
└─ size=0 (no payload)
      └─ type=12 (Disconnect)
         └─ version=1
```

## Payload

**No Payload** - This message consists of the header only.

## Protocol Flow

### Graceful Disconnect

```
Client                                Server
   │                                     │
   │ [User quits game]                   │
   │                                     │
   │───── Disconnect ─────────────────────>│
   │                                     │
   │                  [Remove player]    │
   │                  [Update roster]    │
   │                                     │
   │                  [Broadcast to others]│
   │                                     │
   │                                     │<──── Roster (updated) ────
   │                                     │      (to other clients)
   │                                     │
   │ [Close UDP socket]                  │
   │                                     │
```

### Comparison: Graceful vs Ungraceful

**Graceful (with Disconnect):**
1. Client sends `Disconnect`
2. Server immediately removes player
3. Other clients notified via `Roster` update
4. Clean, fast disconnection

**Ungraceful (without Disconnect):**
1. Client closes socket without notification
2. Server waits for timeout (10-30 seconds)
3. Other clients see "ghost player" until timeout
4. Delayed Roster update

## Client Behavior

### When to Send Disconnect

Client should send `Disconnect` when:
1. **User Quits:** Player exits to menu or closes game
2. **Connection Lost:** Client detects network issue (optional)
3. **Error Occurred:** Fatal error forces disconnect (optional)
4. **Return to Menu:** Server sent `ReturnToMenu` (optional)

**Example Implementation:**
```cpp
void disconnectFromServer() {
    // Send Disconnect notification
    sendDisconnect();

    // Close UDP socket
    socket_.close();

    // Reset state
    connected_ = false;
    my_player_id_ = 0;
    entities_.clear();
    roster_.clear();

    // Return to menu
    showMainMenu();
}

void sendDisconnect() {
    // Prepare header (no payload)
    Header header;
    header.size = 0;
    header.type = MsgType::Disconnect;
    header.version = ProtocolVersion;

    // Send
    try {
        socket_.send_to(asio::buffer(&header, sizeof(Header)), server_endpoint_);
    } catch (const std::exception& e) {
        // If send fails, continue with disconnect anyway
        std::cerr << "Failed to send Disconnect: " << e.what() << "\n";
    }
}
```

### Cleanup Actions

**Before Disconnect:**
```cpp
void gracefulDisconnect() {
    // 1. Send Disconnect
    sendDisconnect();

    // 2. Stop game loop
    stopGameLoop();

    // 3. Close socket
    socket_.close();

    // 4. Clean up resources
    cleanupResources();

    // 5. Update UI
    showDisconnectedState();
}

void cleanupResources() {
    // Clear game state
    entities_.clear();
    roster_.clear();
    scores_.clear();

    // Stop audio
    stopAllSounds();

    // Free textures/resources
    unloadGameAssets();
}
```

## Server Behavior

### Processing Disconnect

**Example Implementation:**
```cpp
void handleDisconnect(const udp::endpoint& sender) {
    // Find player by endpoint
    auto it = endpoint_to_player_id_.find(sender);
    if (it == endpoint_to_player_id_.end()) {
        // Unknown client
        return;
    }

    uint32_t player_id = it->second;

    // Log disconnect
    std::cout << "Player " << player_id << " disconnected gracefully\n";

    // Remove player
    removePlayer(player_id);

    // Broadcast updated Roster
    broadcastRoster();

    // Optional: Check if game should end
    if (players_.size() < MIN_PLAYERS) {
        broadcastReturnToMenu();
    }
}

void removePlayer(uint32_t player_id) {
    // Remove player entity
    entities_.erase(player_id);

    // Remove from roster
    players_.erase(player_id);

    // Remove from tracking maps
    for (auto it = endpoint_to_player_id_.begin(); it != endpoint_to_player_id_.end(); ) {
        if (it->second == player_id) {
            it = endpoint_to_player_id_.erase(it);
        } else {
            ++it;
        }
    }
}
```

### Disconnect vs Timeout

**When Disconnect is received:**
- Immediate removal (< 1 second)
- Clean state cleanup
- Instant Roster update to other clients

**When timeout occurs (no Disconnect):**
- Delayed removal (10-30 seconds)
- Client might have crashed, network issue, or ungraceful exit
- Roster update delayed

## Validation

### Server Validation (After Receiving)

**Required:**
- [ ] `header.version == 1`
- [ ] `header.type == 12` (Disconnect)
- [ ] `header.size == 0`
- [ ] Sender endpoint is known (has a player)

**Example:**
```cpp
bool validateDisconnect(const Header& header, const udp::endpoint& sender) {
    if (header.version != ProtocolVersion) {
        return false;
    }

    if (header.type != MsgType::Disconnect) {
        return false;
    }

    if (header.size != 0) {
        std::cerr << "Disconnect has unexpected payload\n";
        return false;
    }

    // Check if sender is known
    if (endpoint_to_player_id_.find(sender) == endpoint_to_player_id_.end()) {
        std::cerr << "Disconnect from unknown endpoint\n";
        return false;
    }

    return true;
}
```

### Client Validation (Before Sending)

**Required:**
- [ ] Connected to server
- [ ] Have valid player ID

**Example:**
```cpp
void sendDisconnect() {
    if (!connected_ || my_player_id_ == 0) {
        // Not connected, nothing to disconnect from
        return;
    }

    // Send Disconnect
    // ...
}
```

## Wire Format Example

### Disconnect Message

**Complete Message:**
```
Offset  Hex              ASCII   Description
------  --------------   -----   -----------
0x0000  00 00 0C 01      ....    Header (size=0, type=12, version=1)
```

**Total Size:** 4 bytes

**Breakdown:**
- Byte 0: `0x00` - size low byte (0)
- Byte 1: `0x00` - size high byte (0)
- Byte 2: `0x0C` - type (12 = Disconnect)
- Byte 3: `0x01` - version (1)

## Common Issues

### Issue 1: Server Doesn't Remove Player

**Problem:** Client sends Disconnect but server keeps player active

**Causes:**
1. Disconnect message lost (UDP unreliable)
2. Server not handling Disconnect message type
3. Server validation failed

**Solution:**
```cpp
// Client: Send Disconnect multiple times for reliability
void sendDisconnectReliable() {
    for (int i = 0; i < 3; ++i) {
        sendDisconnect();
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
}

// Server: Always handle Disconnect
void handlePacket(const uint8_t* buffer, size_t length, const udp::endpoint& sender) {
    // ... parse header

    switch (header.type) {
        case MsgType::Disconnect:
            handleDisconnect(sender);
            break;
        // ... other cases
    }
}
```

### Issue 2: Client Crashes Before Sending Disconnect

**Problem:** Client crashes, no Disconnect sent

**Solution:** Server must implement timeout mechanism (independent of Disconnect)

```cpp
// Server: Track last activity per client
std::unordered_map<udp::endpoint, std::chrono::steady_clock::time_point> last_activity_;

void onAnyMessageReceived(const udp::endpoint& sender) {
    last_activity_[sender] = std::chrono::steady_clock::now();
}

void checkTimeouts() {
    auto now = std::chrono::steady_clock::now();
    const auto TIMEOUT = std::chrono::seconds(30);

    for (auto it = last_activity_.begin(); it != last_activity_.end(); ) {
        if (now - it->second > TIMEOUT) {
            // Client timed out
            handleDisconnect(it->first);
            it = last_activity_.erase(it);
        } else {
            ++it;
        }
    }
}
```

### Issue 3: Disconnect Sent Multiple Times

**Problem:** Client sends Disconnect repeatedly

**Cause:** Client calls disconnect multiple times (e.g., multiple error handlers)

**Solution:**
```cpp
class Client {
    bool disconnect_sent_ = false;

    void disconnectFromServer() {
        if (disconnect_sent_) {
            return;  // Already disconnected
        }

        sendDisconnect();
        disconnect_sent_ = true;

        // ... cleanup
    }
};
```

## Use Cases

### User Quits to Menu

```cpp
void onQuitButtonPressed() {
    // Show confirmation dialog
    if (confirmQuit()) {
        disconnectFromServer();
        returnToMainMenu();
    }
}
```

### Connection Quality Issues

```cpp
void checkConnectionQuality() {
    auto now = std::chrono::steady_clock::now();
    auto time_since_state = now - last_state_received_;

    if (time_since_state > std::chrono::seconds(10)) {
        // No state received in 10 seconds
        showMessage("Connection lost");
        disconnectFromServer();
    }
}
```

### Application Exit

```cpp
// On application exit
void cleanup() {
    if (connected_) {
        disconnectFromServer();
    }

    // Wait briefly to ensure Disconnect is sent
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Close everything
    closeResources();
}
```

## Best Practices

### 1. Always Send Disconnect

```cpp
// In destructor or shutdown handler
~Client() {
    if (connected_) {
        sendDisconnect();
    }
}
```

### 2. Handle Network Failures

```cpp
void sendDisconnect() {
    try {
        socket_.send_to(asio::buffer(&header, sizeof(Header)), server_endpoint_);
    } catch (const std::exception& e) {
        // Log but don't block disconnect
        std::cerr << "Failed to send Disconnect: " << e.what() << "\n";
    }

    // Continue with local cleanup regardless
}
```

### 3. Multiple Retries (Optional)

```cpp
void sendDisconnectReliable() {
    const int RETRIES = 3;
    const auto DELAY = std::chrono::milliseconds(50);

    for (int i = 0; i < RETRIES; ++i) {
        try {
            sendDisconnect();
        } catch (...) {
            // Ignore errors
        }

        if (i < RETRIES - 1) {
            std::this_thread::sleep_for(DELAY);
        }
    }
}
```

## Testing

### Unit Test Example

```cpp
TEST(Protocol, Disconnect_Format) {
    Header header;
    header.size = 0;
    header.type = MsgType::Disconnect;
    header.version = ProtocolVersion;

    uint8_t buffer[4];
    std::memcpy(buffer, &header, 4);

    // Verify wire format
    EXPECT_EQ(buffer[0], 0x00);  // size low
    EXPECT_EQ(buffer[1], 0x00);  // size high
    EXPECT_EQ(buffer[2], 12);    // type
    EXPECT_EQ(buffer[3], 1);     // version
}

TEST(Protocol, Disconnect_ServerHandling) {
    Server server;
    Client client;

    // Client connects
    client.connect(server);
    ASSERT_TRUE(server.hasPlayer(client.endpoint()));

    // Client disconnects
    client.sendDisconnect();
    server.processMessages();

    // Verify player removed
    EXPECT_FALSE(server.hasPlayer(client.endpoint()));
}
```

### Integration Test

```cpp
TEST(Protocol, GracefulDisconnect) {
    Server server;
    Client client1, client2;

    // Both clients connect
    client1.connect(server);
    client2.connect(server);

    // Verify 2 players
    EXPECT_EQ(server.playerCount(), 2);

    // Client 1 disconnects
    client1.disconnect();
    server.processMessages();

    // Verify 1 player remains
    EXPECT_EQ(server.playerCount(), 1);

    // Client 2 should receive updated Roster
    client2.processMessages();
    EXPECT_EQ(client2.getRosterSize(), 1);
}
```

## Related Documentation

- **[udp-13-return-to-menu.md](udp-13-return-to-menu.md)** - ReturnToMenu message (server-initiated disconnect)
- **[udp-09-roster.md](udp-09-roster.md)** - Roster message (updated after disconnect)
- **[02-transport.md](02-transport.md)** - UDP transport and connection management

---

**Last Updated:** 2025-10-29

