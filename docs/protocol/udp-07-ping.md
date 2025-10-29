# Ping (7) - UDP

## Overview

**Message Type:** `Ping` (7)
**Transport:** UDP
**Direction:** Bidirectional (Client ↔ Server)
**Purpose:** Measure round-trip time (RTT) and keep connection alive
**Status:** ⚠️ **RESERVED** (not currently implemented)

## Current Status

This message type is defined in the protocol but **not currently used**. It is reserved for future implementation of latency measurement and connection keepalive.

**Use Cases:**
1. **Latency Measurement:** Measure network RTT for display to player
2. **Keep-Alive:** Detect disconnected clients (timeout if no Ping/Pong)
3. **Connection Quality:** Track packet loss and jitter
4. **Matchmaking:** Server-side latency-based matchmaking

## Proposed Message Format

### Complete Message Structure (Client → Server)

```
┌─────────────────────────┬───────────────────────┐
│   Header (4 bytes)      │  Payload (8 bytes)    │
├─────────────────────────┼───────────────────────┤
│ size=8, type=7, ver=1   │  Timestamp + Sequence │
└─────────────────────────┴───────────────────────┘
```

**Total Message Size:** 12 bytes

### Header

| Field | Value | Encoding |
|-------|-------|----------|
| `size` | 8 | `uint16_t` (little-endian) |
| `type` | 7 | `uint8_t` (Ping) |
| `version` | 1 | `uint8_t` |

### Payload Structure

**Proposed:**
```cpp
#pragma pack(push, 1)
struct PingPayload {
    std::uint32_t sequence;    // Sequence number (for matching Ping/Pong)
    std::uint32_t timestamp;   // Client timestamp (milliseconds since epoch or session start)
};
#pragma pack(pop)
```

**Alternative (Simpler):**
```cpp
#pragma pack(push, 1)
struct PingPayload {
    std::uint64_t timestamp;   // High-precision timestamp (microseconds)
};
#pragma pack(pop)
```

**Size:** 8 bytes

### Binary Layout (Sequence + Timestamp)

| Offset | Size | Type | Field | Description |
|--------|------|------|-------|-------------|
| 0 | 4 bytes | `uint32_t` | `sequence` | Ping sequence number (little-endian) |
| 4 | 4 bytes | `uint32_t` | `timestamp` | Client timestamp in ms (little-endian) |

## Field Specifications

### sequence

**Type:** `uint32_t` (4 bytes, little-endian)

**Purpose:** Match Ping with corresponding Pong response

**Usage:**
- Client increments for each Ping sent
- Server echoes back in Pong
- Client uses to match Pong to original Ping

**Example:**
```cpp
uint32_t ping_sequence_ = 0;

void sendPing() {
    PingPayload payload;
    payload.sequence = ping_sequence_++;
    payload.timestamp = getCurrentTimeMs();

    sendPacket(MsgType::Ping, &payload, sizeof(payload));

    // Store for RTT calculation
    pending_pings_[payload.sequence] = {payload.timestamp, getCurrentTimeMs()};
}
```

### timestamp

**Type:** `uint32_t` (4 bytes, little-endian)

**Purpose:** Client-side timestamp for RTT calculation

**Format:** Milliseconds since some epoch (session start, Unix epoch, etc.)

**Usage:**
- Client records timestamp when sending Ping
- Server echoes back in Pong (doesn't need to parse it)
- Client calculates RTT: `current_time - timestamp`

**Note:** 32-bit milliseconds overflow after ~49.7 days. For longer sessions, use 64-bit timestamp.

## Protocol Flow

### Latency Measurement

```
Client                                Server
   │                                     │
   │ [Record time: T0]                   │
   │                                     │
   │───── Ping (seq=1, ts=T0) ──────────>│
   │                                     │
   │                  [Receive Ping]     │
   │                  [Echo back]        │
   │                                     │
   │<──── Pong (seq=1, ts=T0) ────────────│
   │                                     │
   │ [Receive time: T1]                  │
   │ [Calculate RTT: T1 - T0]            │
   │ [Display: "Latency: 45ms"]          │
   │                                     │
```

### Keep-Alive

```
Client                                Server
   │                                     │
   │ [Every 5 seconds]                   │
   │                                     │
   │───── Ping ──────────────────────────>│
   │                                     │
   │                  [Reset timeout]    │
   │                                     │
   │<──── Pong ────────────────────────────│
   │                                     │
   │ [Reset timeout]                     │
   │                                     │
   │                                     │
   │ [10 seconds pass, no message]       │
   │                  [Timeout!]         │
   │                  [Remove client]    │
   │                  [Update Roster]    │
   │                                     │
```

### Server-Initiated Ping

Server can also initiate Ping to measure RTT to clients:

```
Client                                Server
   │                                     │
   │                  [Record time: T0]  │
   │                                     │
   │<──── Ping (seq=42, ts=T0) ────────────│
   │                                     │
   │ [Receive Ping]                      │
   │ [Echo back immediately]             │
   │                                     │
   │───── Pong (seq=42, ts=T0) ──────────>│
   │                                     │
   │                  [Receive time: T1] │
   │                  [Calculate RTT]    │
   │                  [Log client latency]│
   │                                     │
```

## Proposed Server Behavior

### Processing Ping

```cpp
void handlePing(const PingPayload& payload, const udp::endpoint& sender) {
    // Simply echo back as Pong
    sendPong(payload, sender);

    // Reset inactivity timeout for this client
    auto it = client_last_activity_.find(sender);
    if (it != client_last_activity_.end()) {
        it->second = getCurrentTime();
    }
}

void sendPong(const PingPayload& original_ping, const udp::endpoint& client) {
    Header header{sizeof(PingPayload), MsgType::Pong, ProtocolVersion};

    uint8_t buffer[sizeof(Header) + sizeof(PingPayload)];
    std::memcpy(buffer, &header, sizeof(Header));
    std::memcpy(buffer + sizeof(Header), &original_ping, sizeof(PingPayload));

    socket_.send_to(asio::buffer(buffer, sizeof(buffer)), client);
}
```

### Timeout Detection

```cpp
const std::chrono::seconds CLIENT_TIMEOUT{30};

void checkClientTimeouts() {
    auto now = getCurrentTime();

    for (auto it = client_last_activity_.begin(); it != client_last_activity_.end(); ) {
        if (now - it->second > CLIENT_TIMEOUT) {
            // Client timed out
            std::cout << "Client " << it->first << " timed out\n";

            // Remove player
            uint32_t player_id = endpoint_to_player_id_[it->first];
            removePlayer(player_id);

            // Remove from tracking
            endpoint_to_player_id_.erase(it->first);
            it = client_last_activity_.erase(it);

            // Broadcast updated Roster
            broadcastRoster();
        } else {
            ++it;
        }
    }
}

// Call in game loop
void tick() {
    static auto last_timeout_check = getCurrentTime();
    auto now = getCurrentTime();

    if (now - last_timeout_check > std::chrono::seconds(5)) {
        checkClientTimeouts();
        last_timeout_check = now;
    }

    // ... rest of tick logic
}
```

## Proposed Client Behavior

### Sending Ping

```cpp
class Client {
    uint32_t ping_sequence_ = 0;
    std::unordered_map<uint32_t, uint32_t> pending_pings_;  // seq -> send_time

    const std::chrono::milliseconds PING_INTERVAL{1000};  // 1 second
    std::chrono::steady_clock::time_point last_ping_time_;

public:
    void update() {
        auto now = std::chrono::steady_clock::now();

        if (now - last_ping_time_ >= PING_INTERVAL) {
            sendPing();
            last_ping_time_ = now;
        }
    }

    void sendPing() {
        PingPayload payload;
        payload.sequence = ping_sequence_++;
        payload.timestamp = getTimeMs();

        // Store for RTT calculation
        pending_pings_[payload.sequence] = payload.timestamp;

        // Send
        Header header{sizeof(PingPayload), MsgType::Ping, ProtocolVersion};
        sendPacket(header, payload);
    }

    void handlePong(const PingPayload& pong) {
        auto it = pending_pings_.find(pong.sequence);
        if (it == pending_pings_.end()) {
            // Unknown Pong (duplicate or timeout)
            return;
        }

        uint32_t send_time = it->second;
        uint32_t receive_time = getTimeMs();
        uint32_t rtt = receive_time - send_time;

        // Update latency statistics
        updateLatencyStats(rtt);

        // Remove from pending
        pending_pings_.erase(it);

        // Display
        std::cout << "Latency: " << rtt << "ms\n";
    }

    void updateLatencyStats(uint32_t rtt) {
        // Exponential moving average
        const float alpha = 0.2f;
        smoothed_rtt_ = alpha * rtt + (1.0f - alpha) * smoothed_rtt_;

        // Track min/max
        min_rtt_ = std::min(min_rtt_, rtt);
        max_rtt_ = std::max(max_rtt_, rtt);

        // Packet loss estimate (if no Pong after timeout)
        // ...
    }
};
```

## Use Cases

### 1. Display Latency to Player

```cpp
// In game UI
void renderUI() {
    drawText(10, 10, "Ping: " + std::to_string(client.getLatency()) + "ms");

    // Color code latency
    Color color = Green;
    if (client.getLatency() > 100) color = Yellow;
    if (client.getLatency() > 200) color = Red;

    drawText(10, 30, "Connection: " + getConnectionQuality(client.getLatency()), color);
}

std::string getConnectionQuality(uint32_t rtt) {
    if (rtt < 50) return "Excellent";
    if (rtt < 100) return "Good";
    if (rtt < 200) return "Fair";
    return "Poor";
}
```

### 2. Server-Side Latency Tracking

```cpp
// Server tracks per-client latency
std::unordered_map<uint32_t, uint32_t> player_latencies_;

void handlePong(uint32_t player_id, const PingPayload& pong) {
    uint32_t send_time = pending_pings_[player_id][pong.sequence];
    uint32_t rtt = getCurrentTimeMs() - send_time;

    player_latencies_[player_id] = rtt;

    // Log high latency players
    if (rtt > 300) {
        std::cout << "Player " << player_id << " has high latency: " << rtt << "ms\n";
    }
}

// Use for lag compensation
void processInput(uint32_t player_id, const InputPacket& input) {
    uint32_t latency = player_latencies_[player_id];

    // Rewind game state by latency amount for fair hit detection
    // (advanced lag compensation)
}
```

### 3. Connection Timeout

```cpp
// Client: Detect server disconnect
class Client {
    std::chrono::steady_clock::time_point last_message_time_;
    const std::chrono::seconds DISCONNECT_TIMEOUT{10};

    void onAnyMessageReceived() {
        last_message_time_ = std::chrono::steady_clock::now();
    }

    void update() {
        auto now = std::chrono::steady_clock::now();

        if (now - last_message_time_ > DISCONNECT_TIMEOUT) {
            // Server disconnected or network issue
            handleDisconnect();
            showMessage("Connection to server lost");
        }
    }
};
```

## Wire Format Example

### Ping Message: Sequence 5, Timestamp 12345ms

**Complete Message:**
```
Offset  Hex                      ASCII   Description
------  ----------------------   -----   -----------
0x0000  08 00 07 01              ....    Header (size=8, type=7, version=1)
0x0004  05 00 00 00              ....    sequence=5
0x0008  39 30 00 00              90..    timestamp=12345 (0x00003039)
```

**Total Size:** 12 bytes

## Performance Characteristics

### Bandwidth

**Per Ping:** 12 bytes
**Per Pong:** 12 bytes
**Round-Trip:** 24 bytes

**Frequency Examples:**
- 1 Hz (every second): 24 bytes/s per client (negligible)
- 10 Hz (every 100ms): 240 bytes/s per client (still minimal)

**Server (N clients @ 1 Hz):**
- Receive Ping: 12 bytes × N
- Send Pong: 12 bytes × N
- Total: 24 bytes/s × N

### Latency Measurement Accuracy

**Factors Affecting Accuracy:**
1. **Client Clock Granularity:** ±1ms (typical)
2. **Send/Receive Processing:** <1ms
3. **Network Jitter:** Variable
4. **UDP Ordering:** Pong may arrive out of order

**Best Practices:**
- Send multiple Pings, average results
- Discard outliers (packet loss, reordering)
- Use exponential moving average

## Testing

### Unit Test Example

```cpp
TEST(Protocol, Ping_Format) {
    Header header{sizeof(PingPayload), MsgType::Ping, ProtocolVersion};
    PingPayload payload{5, 12345};

    uint8_t buffer[12];
    std::memcpy(buffer, &header, 4);
    std::memcpy(buffer + 4, &payload, 8);

    // Verify header
    EXPECT_EQ(buffer[0], 0x08);  // size = 8
    EXPECT_EQ(buffer[2], 7);     // type = Ping

    // Verify sequence
    EXPECT_EQ(buffer[4], 0x05);  // sequence = 5

    // Verify timestamp
    EXPECT_EQ(buffer[8], 0x39);  // 12345 low byte
    EXPECT_EQ(buffer[9], 0x30);
}

TEST(Protocol, RTT_Calculation) {
    Client client;

    // Send Ping at T=1000
    client.sendPing();

    // Simulate Pong received at T=1050 (50ms RTT)
    client.handlePong(/* ... */);

    EXPECT_EQ(client.getLatency(), 50);
}
```

## Implementation Checklist

**Server:**
- [ ] Handle Ping message type
- [ ] Echo Ping as Pong (copy payload)
- [ ] Update client activity timestamp
- [ ] Implement timeout detection
- [ ] Remove inactive clients
- [ ] Optional: Send server-initiated Pings

**Client:**
- [ ] Send Ping periodically (e.g., 1 Hz)
- [ ] Store pending Pings (sequence → timestamp)
- [ ] Handle Pong message type
- [ ] Calculate and display RTT
- [ ] Track latency statistics (min/max/average)
- [ ] Detect server timeout

**Testing:**
- [ ] Test RTT calculation accuracy
- [ ] Test timeout detection (stop sending Input, verify disconnect)
- [ ] Test with packet loss
- [ ] Test with high latency

## Related Documentation

- **[udp-08-pong.md](udp-08-pong.md)** - Pong message (response to Ping)
- **[00-overview.md](00-overview.md)** - Protocol overview

---

**Last Updated:** 2025-10-29
**Status:** ⚠️ Reserved for future implementation

