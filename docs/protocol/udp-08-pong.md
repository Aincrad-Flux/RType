# Pong (8) - UDP

## Overview

**Message Type:** `Pong` (8)
**Transport:** UDP
**Direction:** Bidirectional (Client ↔ Server)
**Purpose:** Response to Ping message for RTT measurement
**Status:** ⚠️ **RESERVED** (not currently implemented)

## Current Status

This message type is defined in the protocol but **not currently used**. It is reserved for future implementation as the response to `Ping` messages.

**Purpose:** Echo back the Ping payload to allow RTT (round-trip time) calculation.

## Proposed Message Format

### Complete Message Structure

```
┌─────────────────────────┬───────────────────────┐
│   Header (4 bytes)      │  Payload (8 bytes)    │
├─────────────────────────┼───────────────────────┤
│ size=8, type=8, ver=1   │  Echoed Ping Payload  │
└─────────────────────────┴───────────────────────┘
```

**Total Message Size:** 12 bytes

### Header

| Field | Value | Encoding |
|-------|-------|----------|
| `size` | 8 | `uint16_t` (little-endian) |
| `type` | 8 | `uint8_t` (Pong) |
| `version` | 1 | `uint8_t` |

### Payload: Echoed PingPayload

**Structure:** Identical to Ping payload

```cpp
#pragma pack(push, 1)
struct PongPayload {
    std::uint32_t sequence;    // Echoed from Ping
    std::uint32_t timestamp;   // Echoed from Ping
};
#pragma pack(pop)
```

**Size:** 8 bytes

**Important:** Pong payload is a **verbatim copy** of the Ping payload. The responder does not modify the fields.

### Binary Layout

| Offset | Size | Type | Field | Description |
|--------|------|------|-------|-------------|
| 0 | 4 bytes | `uint32_t` | `sequence` | Echoed sequence number (little-endian) |
| 4 | 4 bytes | `uint32_t` | `timestamp` | Echoed timestamp (little-endian) |

## Field Specifications

### sequence

**Type:** `uint32_t` (4 bytes, little-endian)

**Purpose:** Match Pong to original Ping

**Value:** Exact copy from Ping message

**Receiver Usage:**
- Look up pending Ping by sequence
- Calculate RTT using stored send time
- Detect duplicate or stale Pongs

### timestamp

**Type:** `uint32_t` (4 bytes, little-endian)

**Purpose:** Allow sender to calculate RTT

**Value:** Exact copy from Ping message

**Note:** Responder does NOT need to parse or understand this field; just echo it back.

**Sender Usage:**
- Compare with current time to calculate RTT
- Validate timestamp is recent (detect stale Pongs)

## Protocol Flow

### Simple Echo Pattern

```
Sender (A)                          Responder (B)
   │                                     │
   │ [T0: Record send time]              │
   │                                     │
   │───── Ping (seq=N, ts=T0) ──────────>│
   │                                     │
   │              [Receive Ping]         │
   │              [Copy payload]         │
   │              [Send back]            │
   │                                     │
   │<──── Pong (seq=N, ts=T0) ────────────│
   │                                     │
   │ [T1: Record receive time]           │
   │ [RTT = T1 - T0]                     │
   │                                     │
```

**Key Property:** Responder is stateless; no need to track Ping history.

## Proposed Server Behavior (Responding to Client Ping)

### Processing Client Ping

```cpp
void handlePing(const PingPayload& payload, const udp::endpoint& sender) {
    // Echo back as Pong (verbatim copy)
    sendPong(payload, sender);

    // Optional: Update client activity tracking
    resetClientTimeout(sender);
}

void sendPong(const PingPayload& original_ping, const udp::endpoint& client) {
    // Prepare header (change type to Pong)
    Header header;
    header.size = sizeof(PingPayload);
    header.type = MsgType::Pong;
    header.version = ProtocolVersion;

    // Copy payload verbatim
    uint8_t buffer[sizeof(Header) + sizeof(PingPayload)];
    std::memcpy(buffer, &header, sizeof(Header));
    std::memcpy(buffer + sizeof(Header), &original_ping, sizeof(PingPayload));

    // Send
    socket_.send_to(asio::buffer(buffer, sizeof(buffer)), client);
}
```

**Important:** Server does NOT modify `sequence` or `timestamp` fields.

## Proposed Client Behavior (Responding to Server Ping)

### Processing Server Ping

If the server initiates Ping, the client responds with Pong:

```cpp
void handlePing(const PingPayload& payload) {
    // Echo back as Pong
    sendPong(payload);
}

void sendPong(const PingPayload& original_ping) {
    Header header{sizeof(PingPayload), MsgType::Pong, ProtocolVersion};

    uint8_t buffer[sizeof(Header) + sizeof(PingPayload)];
    std::memcpy(buffer, &header, sizeof(Header));
    std::memcpy(buffer + sizeof(Header), &original_ping, sizeof(PingPayload));

    socket_.send_to(asio::buffer(buffer, sizeof(buffer)), server_endpoint_);
}
```

### Processing Pong (Response to Own Ping)

```cpp
struct PingRecord {
    uint32_t send_time_ms;
    std::chrono::steady_clock::time_point send_time_point;
};

std::unordered_map<uint32_t, PingRecord> pending_pings_;

void handlePong(const PongPayload& pong) {
    // Find matching Ping
    auto it = pending_pings_.find(pong.sequence);
    if (it == pending_pings_.end()) {
        // Unknown Pong (duplicate, timeout, or out-of-order)
        return;
    }

    // Calculate RTT
    auto now = std::chrono::steady_clock::now();
    auto rtt = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - it->second.send_time_point
    ).count();

    // Alternative: Use timestamp (if synchronized clocks)
    // uint32_t rtt = getCurrentTimeMs() - pong.timestamp;

    // Update statistics
    updateLatency(rtt);

    // Remove from pending
    pending_pings_.erase(it);

    // Display
    std::cout << "Latency: " << rtt << "ms (seq=" << pong.sequence << ")\n";
}

void updateLatency(uint32_t rtt_ms) {
    // Exponential moving average
    const float alpha = 0.2f;
    if (latency_samples_ == 0) {
        avg_latency_ = rtt_ms;
    } else {
        avg_latency_ = alpha * rtt_ms + (1.0f - alpha) * avg_latency_;
    }

    min_latency_ = std::min(min_latency_, rtt_ms);
    max_latency_ = std::max(max_latency_, rtt_ms);
    latency_samples_++;
}
```

## Validation

### Responder Validation (Before Echoing)

**Required:**
- [ ] `header.version == 1`
- [ ] `header.type == 7` (Ping)
- [ ] `header.size == 8`

**NO validation of payload fields** (just echo them back)

### Sender Validation (After Receiving Pong)

**Required:**
- [ ] `header.version == 1`
- [ ] `header.type == 8` (Pong)
- [ ] `header.size == 8`
- [ ] `pong.sequence` matches a pending Ping

**Optional:**
- [ ] `pong.timestamp` is recent (not stale)
- [ ] RTT is reasonable (not negative, not > 10 seconds)

**Example:**
```cpp
bool validatePong(const PongPayload& pong, uint32_t current_time_ms) {
    // Check if sequence is known
    if (pending_pings_.find(pong.sequence) == pending_pings_.end()) {
        std::cerr << "Pong for unknown sequence: " << pong.sequence << "\n";
        return false;
    }

    // Optional: Check timestamp sanity
    if (pong.timestamp > current_time_ms) {
        std::cerr << "Pong timestamp is in the future!\n";
        return false;
    }

    uint32_t rtt = current_time_ms - pong.timestamp;
    if (rtt > 10000) {  // 10 seconds
        std::cerr << "Pong RTT too high: " << rtt << "ms (likely stale)\n";
        return false;
    }

    return true;
}
```

## Wire Format Example

### Pong Message: Echoing Sequence 5, Timestamp 12345ms

**Complete Message:**
```
Offset  Hex                      ASCII   Description
------  ----------------------   -----   -----------
0x0000  08 00 08 01              ....    Header (size=8, type=8, version=1)
0x0004  05 00 00 00              ....    sequence=5 (echoed)
0x0008  39 30 00 00              90..    timestamp=12345 (echoed)
```

**Total Size:** 12 bytes

**Note:** Payload is **identical** to the Ping that triggered this Pong.

## Common Issues

### Issue 1: Pong Never Arrives

**Problem:** Sent Ping but never received Pong

**Causes:**
1. **Packet Loss:** UDP Ping or Pong lost in transit
2. **Responder Not Implementing:** Server/client doesn't handle Ping
3. **Firewall:** Blocking UDP packets

**Debug:**
```bash
# Capture UDP traffic
sudo tcpdump -i any udp port 4242 -X

# Verify Ping is sent (type=0x07)
# Verify Pong is received (type=0x08)
```

**Solution:**
```cpp
// Implement timeout for pending Pings
const std::chrono::seconds PING_TIMEOUT{5};

void cleanupPendingPings() {
    auto now = std::chrono::steady_clock::now();

    for (auto it = pending_pings_.begin(); it != pending_pings_.end(); ) {
        if (now - it->second.send_time_point > PING_TIMEOUT) {
            // Timeout, assume packet lost
            packet_loss_count_++;
            it = pending_pings_.erase(it);
        } else {
            ++it;
        }
    }
}
```

### Issue 2: RTT Calculation Incorrect

**Problem:** Displayed latency doesn't match real latency

**Causes:**
1. **Clock Desync:** Client and server clocks not synchronized (if using timestamp directly)
2. **Endianness:** Byte order mismatch
3. **Overflow:** Timestamp wrapped around (uint32 ms overflows after 49 days)

**Solution:**
```cpp
// Use local monotonic clock, not echoed timestamp
struct PingRecord {
    std::chrono::steady_clock::time_point send_time;  // Local clock
    uint32_t echoed_timestamp;  // For validation only
};

void handlePong(const PongPayload& pong) {
    auto it = pending_pings_.find(pong.sequence);
    if (it == pending_pings_.end()) return;

    // Calculate RTT using local clock (accurate)
    auto rtt = std::chrono::steady_clock::now() - it->second.send_time;
    auto rtt_ms = std::chrono::duration_cast<std::chrono::milliseconds>(rtt).count();

    // Optional: Validate echoed timestamp matches (detect corruption)
    if (pong.timestamp != it->second.echoed_timestamp) {
        std::cerr << "Timestamp mismatch in Pong!\n";
    }

    updateLatency(rtt_ms);
    pending_pings_.erase(it);
}
```

### Issue 3: Duplicate Pongs

**Problem:** Receive multiple Pongs for same Ping

**Causes:**
1. **UDP Duplication:** Network duplicated the packet (rare)
2. **Responder Bug:** Sent Pong multiple times

**Solution:**
```cpp
void handlePong(const PongPayload& pong) {
    auto it = pending_pings_.find(pong.sequence);
    if (it == pending_pings_.end()) {
        // Already processed or unknown
        return;  // Silently ignore duplicate
    }

    // Process first Pong
    // ...

    // Remove from pending (subsequent Pongs will be ignored)
    pending_pings_.erase(it);
}
```

## Performance Characteristics

### Bandwidth

**Per Pong:** 12 bytes

**Same as Ping:** See [udp-07-ping.md](udp-07-ping.md#performance-characteristics)

### Latency

- Pong is sent immediately upon Ping receipt (<1ms processing)
- Network transit time: variable
- Total RTT: 2 × network_latency + processing_overhead

## Use Cases

### 1. Display Network Quality

```cpp
void renderNetworkIndicator() {
    uint32_t latency = client.getAvgLatency();

    if (latency < 50) {
        drawIcon("wifi_excellent");
    } else if (latency < 100) {
        drawIcon("wifi_good");
    } else if (latency < 200) {
        drawIcon("wifi_fair");
    } else {
        drawIcon("wifi_poor");
    }

    drawText("Ping: " + std::to_string(latency) + "ms");
}
```

### 2. Lag Compensation

```cpp
// Server adjusts hit detection based on player latency
void processShot(uint32_t shooter_id, float target_x, float target_y) {
    uint32_t shooter_latency = getPlayerLatency(shooter_id);

    // Rewind game state by shooter's latency
    rewindGameState(shooter_latency);

    // Check if shot hit target
    bool hit = checkCollision(target_x, target_y);

    // Restore current game state
    restoreGameState();

    if (hit) {
        applyDamage(target_id);
    }
}
```

### 3. Adaptive Quality

```cpp
// Client adjusts prediction/interpolation based on latency
void updatePrediction() {
    uint32_t latency = client.getAvgLatency();

    if (latency < 50) {
        // Low latency: minimal prediction
        prediction_time_ = 0.0f;
    } else if (latency < 150) {
        // Medium latency: moderate prediction
        prediction_time_ = latency / 1000.0f;
    } else {
        // High latency: aggressive prediction
        prediction_time_ = latency / 1000.0f * 1.5f;
    }
}
```

## Testing

### Unit Test Example

```cpp
TEST(Protocol, Pong_Echo) {
    // Original Ping payload
    PingPayload ping{42, 12345};

    // Server echoes as Pong
    Header pong_header{sizeof(PingPayload), MsgType::Pong, ProtocolVersion};
    PongPayload pong = ping;  // Verbatim copy

    // Verify echo
    EXPECT_EQ(pong.sequence, ping.sequence);
    EXPECT_EQ(pong.timestamp, ping.timestamp);

    // Verify header changed to Pong
    EXPECT_EQ(pong_header.type, MsgType::Pong);
}

TEST(Protocol, RTT_Measurement) {
    Client client;

    // Send Ping
    auto start = std::chrono::steady_clock::now();
    client.sendPing();

    // Simulate 50ms delay
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // Receive Pong
    client.handlePong(/* echoed payload */);

    // Verify RTT is approximately 50ms (±5ms tolerance)
    EXPECT_NEAR(client.getLatency(), 50, 5);
}
```

### Integration Test

```cpp
TEST(Protocol, Ping_Pong_Cycle) {
    // Setup
    Server server;
    Client client;

    // Client sends Ping
    uint32_t seq = client.sendPing();

    // Server receives and responds
    server.processMessages();  // Handles Ping, sends Pong

    // Client receives Pong
    client.processMessages();

    // Verify RTT was calculated
    EXPECT_GT(client.getLatency(), 0);

    // Verify pending Ping was removed
    EXPECT_FALSE(client.hasPendingPing(seq));
}
```

## Implementation Checklist

**Server:**
- [ ] Handle Ping message type
- [ ] Copy Ping payload verbatim
- [ ] Send Pong with copied payload
- [ ] Optional: Track client latencies

**Client:**
- [ ] Handle Pong message type
- [ ] Match Pong to pending Ping by sequence
- [ ] Calculate RTT using local clock
- [ ] Update latency statistics
- [ ] Clean up stale pending Pings (timeout)

**Testing:**
- [ ] Test echo correctness (payload unchanged)
- [ ] Test RTT calculation accuracy
- [ ] Test with packet loss (Ping or Pong lost)
- [ ] Test with duplicate Pongs

## Related Documentation

- **[udp-07-ping.md](udp-07-ping.md)** - Ping message (triggers Pong response)
- **[00-overview.md](00-overview.md)** - Protocol overview

---

**Last Updated:** 2025-10-29
**Status:** ⚠️ Reserved for future implementation

