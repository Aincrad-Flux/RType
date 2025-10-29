# HelloAck (2) - UDP

## Overview

**Message Type:** `HelloAck` (2)
**Transport:** UDP
**Direction:** Server → Client
**Purpose:** Acknowledges successful UDP session establishment
**Status:** Active
**Frequency:** Once per client, in response to Hello

## Message Format

### Complete Message Structure

```
┌─────────────────────────┐
│   Header (4 bytes)      │
│  (No Payload)           │
├─────────────────────────┤
│ size=0, type=2, ver=1   │
└─────────────────────────┘
```

**Total Message Size:** 4 bytes (header only)

### Header

| Field | Value | Encoding |
|-------|-------|----------|
| `size` | 0 | `uint16_t` (little-endian) |
| `type` | 2 | `uint8_t` (HelloAck) |
| `version` | 1 | `uint8_t` |

**Wire Format:**
```
00 00 02 01
└─ size=0 (no payload)
      └─ type=2 (HelloAck)
         └─ version=1
```

## Payload

**No Payload** - This message consists of the header only.

## Protocol Flow

### UDP Session Acknowledgment

```
Client                                Server
   │                                     │
   │───── Hello (token, name) ──────────>│
   │                                     │
   │                  [Validate token]   │
   │                  [Create player]    │
   │                                     │
   │<──── HelloAck ───────────────────────│  ✓ Confirmed!
   │                                     │
   │ [UDP session established]           │
   │ [Ready to send Input]               │
   │ [Ready to receive State]            │
   │                                     │
   │<──── State (includes player) ────────│
   │<──── Roster (all players) ───────────│
   │                                     │
```

**Timing:**
- Sent immediately after server processes valid Hello
- Typically within milliseconds of receiving Hello
- Sent to the same endpoint that sent Hello

## Server Behavior

### When to Send HelloAck

The server sends `HelloAck` after:
1. Receiving a valid `Hello` message
2. Successfully validating the authentication token
3. Creating or updating the player entity
4. Associating the UDP endpoint with the player ID

**Code Example:**
```cpp
void handleHello(const UdpHelloPayload& payload, const udp::endpoint& sender) {
    // 1. Validate token
    if (!isValidToken(payload.token)) {
        return;  // Silent failure
    }

    // 2. Create/update player
    uint32_t player_id = getOrCreatePlayer(sender, payload.name);

    // 3. Send HelloAck
    sendHelloAck(sender);

    // 4. Player is now active, will be included in State broadcasts
}
```

## Client Behavior

### On Receiving HelloAck

The client should:
1. **Mark UDP Session as Established:**
   - Set internal state to "connected"
   - Stop retrying Hello messages

2. **Start Gameplay Loop:**
   - Begin sending Input messages
   - Begin processing State messages
   - Update UI to show "Connected" or "In Game"

3. **Start Timeout Timer:**
   - Monitor for State messages
   - If no State received for N seconds (e.g., 10), show "Connection Lost"

**Example:**
```cpp
void handleHelloAck() {
    // Mark as connected
    udp_connected_ = true;

    // Stop Hello retry timer
    hello_retry_timer_.cancel();

    // Start gameplay
    startGameLoop();

    // Start connection monitoring
    startHeartbeatMonitor();

    // Update UI
    showGameScreen();
}
```

## Implementation Examples

### Server (Sending)

```cpp
#include "common/Protocol.hpp"
#include <asio.hpp>
#include <cstring>

using namespace rtype::net;

void sendHelloAck(asio::ip::udp::socket& socket,
                  const asio::ip::udp::endpoint& client_endpoint) {
    // Prepare header (no payload)
    Header header;
    header.size = 0;
    header.type = MsgType::HelloAck;
    header.version = ProtocolVersion;

    // Send header only
    socket.send_to(asio::buffer(&header, sizeof(Header)), client_endpoint);
}
```

### Client (Receiving)

```cpp
#include "common/Protocol.hpp"
#include <asio.hpp>

using namespace rtype::net;

bool isHelloAck(const uint8_t* buffer, size_t length) {
    // Validate size
    if (length < sizeof(Header)) {
        return false;
    }

    // Parse header
    Header header;
    std::memcpy(&header, buffer, sizeof(Header));

    // Check type and version
    return header.type == MsgType::HelloAck &&
           header.version == ProtocolVersion &&
           header.size == 0;
}

void receiveLoop() {
    uint8_t buffer[2048];
    udp::endpoint sender;

    while (running_) {
        size_t length = socket_.receive_from(asio::buffer(buffer), sender);

        if (isHelloAck(buffer, length)) {
            handleHelloAck();
        } else {
            // Handle other message types
            routeMessage(buffer, length);
        }
    }
}
```

## Validation

### Server Validation (Before Sending)

✅ **No validation needed** - server sends header-only message

**Best Practices:**
- Only send HelloAck after successful Hello processing
- Send to correct endpoint (same as Hello sender)
- Log HelloAck sending for debugging

### Client Validation (After Receiving)

**Required:**
- [ ] `length >= 4` (minimum header size)
- [ ] `header.version == 1`
- [ ] `header.type == 2` (HelloAck)
- [ ] `header.size == 0` (no payload expected)
- [ ] Sender endpoint matches server endpoint (optional but recommended)

**Example:**
```cpp
bool validateHelloAck(const Header& header, const udp::endpoint& sender) {
    // Version check
    if (header.version != ProtocolVersion) {
        std::cerr << "Wrong protocol version\n";
        return false;
    }

    // Type check
    if (header.type != MsgType::HelloAck) {
        return false;  // Not a HelloAck
    }

    // Size check
    if (header.size != 0) {
        std::cerr << "HelloAck has unexpected payload\n";
        return false;
    }

    // Sender check (optional)
    if (sender != server_endpoint_) {
        std::cerr << "HelloAck from unexpected sender\n";
        return false;
    }

    return true;
}
```

## Wire Format Example

### Complete Message

**Hex Dump:**
```
Offset  Hex              ASCII   Description
------  --------------   -----   -----------
0x0000  00 00 02 01      ....    Header (size=0, type=2, version=1)
```

**Total Size:** 4 bytes

**Breakdown:**
- Byte 0: `0x00` - size low byte (0)
- Byte 1: `0x00` - size high byte (0)
- Byte 2: `0x02` - type (2 = HelloAck)
- Byte 3: `0x01` - version (1)

## Common Issues

### Issue 1: Client Never Receives HelloAck

**Problem:** Client sends Hello but HelloAck never arrives

**Possible Causes:**
1. **Invalid Token:** Server rejected Hello due to bad token
2. **Packet Loss:** HelloAck lost in transit (UDP unreliable)
3. **Firewall:** Server can send, but client firewall blocks inbound UDP
4. **Wrong Endpoint:** Client listening on different port than it sent from

**Debug Steps:**
```bash
# Server-side: Verify HelloAck is being sent
# Add logging before send_to call

# Network capture on client machine
sudo tcpdump -i any udp -X src host SERVER_IP

# Check for 4-byte UDP packet with: 00 00 02 01
```

**Solution:**
```cpp
// Client: Implement retry logic
if (!waitForHelloAck(5s)) {
    // Retry Hello
    sendHello();
}
```

### Issue 2: Multiple HelloAck Received

**Problem:** Client receives multiple HelloAck messages

**Causes:**
1. Client sent multiple Hello messages (retry logic)
2. Server sent multiple HelloAck (bug in server)
3. UDP packet duplication (rare, but possible)

**Solution:**
```cpp
// Client: Ignore duplicate HelloAck
if (udp_connected_) {
    return;  // Already connected, ignore
}

udp_connected_ = true;
// ... proceed with connection logic
```

### Issue 3: HelloAck from Wrong Server

**Problem:** Client receives HelloAck from unexpected source

**Causes:**
1. Multiple servers on network
2. Malicious actor impersonating server
3. Client connected to wrong server

**Solution:**
```cpp
// Always validate sender endpoint
if (sender_endpoint != server_endpoint_) {
    std::cerr << "HelloAck from unexpected endpoint: "
              << sender_endpoint << "\n";
    return;  // Ignore
}
```

## Security Considerations

### Spoofing

**Risk:** Attacker sends fake HelloAck to client

**Impact:**
- Client thinks it's connected but server never created player
- Client sends Input messages that are ignored
- Client never receives State messages (game doesn't work)

**Mitigation:**
```cpp
// 1. Validate sender IP/port
if (sender != expected_server) {
    return;  // Ignore
}

// 2. Expect State message shortly after HelloAck
start_timeout_timer(10s);
if (!received_state) {
    disconnect();
    show_error("Server not responding");
}

// 3. Use sequence numbers or challenge-response
```

### Denial of Service

**Risk:** Attacker floods client with HelloAck messages

**Impact:**
- Minimal (HelloAck is small, client can ignore extras)
- May trigger log spam or unnecessary processing

**Mitigation:**
```cpp
// Rate limit HelloAck processing
if (recently_processed_hello_ack_) {
    return;  // Ignore duplicate within time window
}

recently_processed_hello_ack_ = true;
setTimeout(1s, []() { recently_processed_hello_ack_ = false; });
```

## Performance Characteristics

### Message Size
- **4 bytes** per client (minimal overhead)
- Sent once per session
- Negligible bandwidth impact

### Latency
- Single UDP datagram
- No payload to serialize
- Typical send time: <1ms
- Network transit: 1-100ms depending on distance

## Testing

### Unit Test Example

```cpp
TEST(Protocol, HelloAck_Format) {
    Header header;
    header.size = 0;
    header.type = MsgType::HelloAck;
    header.version = ProtocolVersion;

    uint8_t buffer[4];
    std::memcpy(buffer, &header, 4);

    // Verify wire format
    EXPECT_EQ(buffer[0], 0x00);  // size low
    EXPECT_EQ(buffer[1], 0x00);  // size high
    EXPECT_EQ(buffer[2], 2);     // type
    EXPECT_EQ(buffer[3], 1);     // version
}

TEST(Protocol, HelloAck_Validation) {
    Header header{0, MsgType::HelloAck, ProtocolVersion};

    // Valid HelloAck
    EXPECT_TRUE(isValidHelloAck(header));

    // Wrong version
    header.version = 2;
    EXPECT_FALSE(isValidHelloAck(header));

    // Wrong type
    header.version = 1;
    header.type = MsgType::Hello;
    EXPECT_FALSE(isValidHelloAck(header));

    // Has payload (unexpected)
    header.type = MsgType::HelloAck;
    header.size = 10;
    EXPECT_FALSE(isValidHelloAck(header));
}
```

### Integration Test

```cpp
TEST(Protocol, UdpHandshake) {
    // Setup client and server
    UdpClient client;
    UdpServer server;

    // Server generates token
    uint32_t token = server.generateToken();

    // Client sends Hello
    client.sendHello(token, "TestPlayer");

    // Server processes Hello
    server.processMessages();

    // Client should receive HelloAck
    ASSERT_TRUE(client.waitForMessage(MsgType::HelloAck, 1s));

    // Client should be connected
    EXPECT_TRUE(client.isConnected());

    // Server should have created player
    EXPECT_TRUE(server.hasPlayer(client.endpoint()));
}
```

### Packet Capture

```bash
# Capture UDP on port 4242
sudo tcpdump -i any udp port 4242 -X -s 0

# Look for HelloAck pattern:
# 4-byte packet with hex: 00 00 02 01

# Example output:
# 10:30:45.123456 IP server.4242 > client.54321: UDP, length 4
# 0x0000:  00 00 02 01                                ....
```

## Sequence Diagram

```
┌────────┐                              ┌────────┐
│ Client │                              │ Server │
└───┬────┘                              └───┬────┘
    │                                       │
    │  Hello (token="12345678", name="Alice")
    │──────────────────────────────────────>│
    │                                       │
    │                        [validate token]│
    │                        [create player] │
    │                        [player_id=42]  │
    │                                       │
    │         HelloAck (header only)        │
    │<──────────────────────────────────────│
    │                                       │
    │  [UDP session established]            │
    │  [Start sending Input]                │
    │                                       │
    │<══════════════════════════════════════│
    │    State (includes player 42)         │
    │<══════════════════════════════════════│
    │                                       │
```

## Related Documentation

- **[udp-01-hello.md](udp-01-hello.md)** - Hello message (triggers this response)
- **[udp-04-state.md](udp-04-state.md)** - State messages (follow after HelloAck)
- **[udp-03-input.md](udp-03-input.md)** - Input messages (client sends after HelloAck)
- **[02-transport.md](02-transport.md)** - UDP transport details

---

**Last Updated:** 2025-10-29

