# Hello (1) - UDP

## Overview

**Message Type:** `Hello` (1)
**Transport:** UDP
**Direction:** Client → Server
**Purpose:** UDP session establishment with authentication token
**Status:** Active
**Frequency:** Once at connection start (may retry if no response)

## Message Format

### Complete Message Structure

```
┌─────────────────────────┬───────────────────────┐
│   Header (4 bytes)      │  Payload (20 bytes)   │
├─────────────────────────┼───────────────────────┤
│ size=20, type=1, ver=1  │  UdpHelloPayload      │
└─────────────────────────┴───────────────────────┘
```

**Total Message Size:** 24 bytes

### Header

| Field | Value | Encoding |
|-------|-------|----------|
| `size` | 20 | `uint16_t` (little-endian) |
| `type` | 1 | `uint8_t` (Hello) |
| `version` | 1 | `uint8_t` |

**Wire Format (Header):**
```
14 00 01 01
└─ size=20 (0x14)
      └─ type=1
         └─ version=1
```

### Payload: UdpHelloPayload

**Structure:**
```cpp
#pragma pack(push, 1)
struct UdpHelloPayload {
    std::uint32_t token;  // Must match token from TCP HelloAck
    char name[16];        // Optional username (null-terminated/truncated)
};
#pragma pack(pop)
```

**Size:** 20 bytes

**Binary Layout:**

| Offset | Size | Type | Field | Description |
|--------|------|------|-------|-------------|
| 0 | 4 bytes | `uint32_t` | `token` | Authentication token (little-endian) |
| 4 | 16 bytes | `char[16]` | `name` | Player name (null-terminated UTF-8) |

## Field Specifications

### token

**Type:** `uint32_t` (4 bytes, little-endian)

**Purpose:** Authenticates the UDP session, must match token from TCP `HelloAck`

**Source:** Received in `TcpWelcome` message during TCP handshake

**Validation:**
- Server checks token against expected value for this endpoint
- If invalid, server **silently ignores** the Hello (no error response)

**Example:**

Token 0x12345678:
```
Wire: 78 56 34 12
      └─ 0x12345678 in little-endian
```

### name

**Type:** `char[16]` (16 bytes, null-terminated UTF-8 string)

**Purpose:** Player's display name for the game session

**Constraints:**
- **Maximum 15 displayable characters** + null terminator
- UTF-8 encoded
- Null-terminated within 16 bytes
- If name exceeds 15 characters, client must truncate and add null terminator at byte 15

**Handling:**
- Empty string allowed (just `\0` at byte 0)
- Unused bytes should be zero-filled
- Server may use default name if empty

**Example:**

Name "Alice":
```
Wire: 41 6C 69 63 65 00 00 00 00 00 00 00 00 00 00 00
      └─ "Alice\0" (5 chars + null, zero-padded to 16 bytes)
```

Name "VeryLongPlayerName" (truncated):
```
Original: "VeryLongPlayerName" (18 chars)
Truncated: "VeryLongPlayerN\0" (15 chars + null)
Wire: 56 65 72 79 4C 6F 6E 67 50 6C 61 79 65 72 4E 00
```

## Protocol Flow

### UDP Session Establishment

```
Client                                Server
   │                                     │
   │ [Received token from TCP]           │
   │                                     │
   │───── UDP Hello (token, name) ───────>│
   │                                     │
   │                  [Validate token]   │
   │                  [Create player]    │
   │                  [Assign entity ID] │
   │                                     │
   │<──── UDP HelloAck ────────────────────│
   │                                     │
   │<──── State (includes new player) ─────│
   │<──── Roster (all players) ────────────│
   │                                     │
```

**Timing:**
- Send immediately after receiving token from TCP `TcpWelcome`
- If no `HelloAck` received within timeout (e.g., 5 seconds), retry
- Limit retries to avoid flooding (e.g., max 3 attempts)

## Server Behavior

### On Receiving Hello

1. **Parse Message:**
   - Validate header (size, type, version)
   - Extract token and name from payload

2. **Validate Token:**
   - Check if token matches expected value
   - If invalid: silently ignore message, return early
   - If valid: proceed

3. **Associate Endpoint:**
   - Map sender's `udp::endpoint` (IP + port) to this player
   - If endpoint already known, update player info
   - If new endpoint, create new player

4. **Create Player Entity:**
   - Generate unique entity ID
   - Initialize player position (e.g., x=50, y=100)
   - Initialize player color (e.g., 0x55AAFFFF)
   - Set player name from payload (or default if empty)
   - Set initial lives count (e.g., 3)

5. **Send HelloAck:**
   - Send `HelloAck` message to this endpoint
   - Confirms successful UDP session establishment

6. **Broadcast State:**
   - Include new player in next `State` broadcast
   - Send updated `Roster` to all clients

### Current Implementation (from existing protocol docs)

**Server Actions:**
- Associates UDP endpoint with unique `playerId`
- Creates Player entity at position (x=50, y=100 + offset)
- Assigns color RGBA 0x55AAFFFF
- Initializes input bits to 0

## Implementation Examples

### Client (Sending)

```cpp
#include "common/Protocol.hpp"
#include <asio.hpp>
#include <cstring>

using namespace rtype::net;

void sendUdpHello(asio::ip::udp::socket& socket,
                  const asio::ip::udp::endpoint& server_endpoint,
                  uint32_t token,
                  const std::string& player_name) {
    // Prepare header
    Header header;
    header.size = sizeof(UdpHelloPayload);
    header.type = MsgType::Hello;
    header.version = ProtocolVersion;

    // Prepare payload
    UdpHelloPayload payload{};
    payload.token = token;

    // Copy name (truncate if needed)
    size_t copy_len = std::min(player_name.size(), size_t(15));
    std::memcpy(payload.name, player_name.c_str(), copy_len);
    payload.name[copy_len] = '\0';  // Ensure null termination
    // Remaining bytes are already zero-initialized

    // Combine into buffer
    uint8_t buffer[sizeof(Header) + sizeof(UdpHelloPayload)];
    std::memcpy(buffer, &header, sizeof(Header));
    std::memcpy(buffer + sizeof(Header), &payload, sizeof(UdpHelloPayload));

    // Send via UDP
    socket.send_to(asio::buffer(buffer, sizeof(buffer)), server_endpoint);
}
```

### Server (Receiving)

```cpp
#include "common/Protocol.hpp"
#include <asio.hpp>
#include <cstring>
#include <unordered_map>

using namespace rtype::net;

class UdpServer {
    std::unordered_map<uint32_t, asio::ip::udp::endpoint> token_to_endpoint_;
    std::unordered_map<asio::ip::udp::endpoint, uint32_t> endpoint_to_player_id_;
    uint32_t next_player_id_ = 1;

    void handleHello(const UdpHelloPayload& payload,
                     const asio::ip::udp::endpoint& sender) {
        // Validate token
        if (token_to_endpoint_.find(payload.token) == token_to_endpoint_.end()) {
            // Invalid token, ignore
            return;
        }

        // Check if this endpoint is already known
        uint32_t player_id;
        auto it = endpoint_to_player_id_.find(sender);
        if (it != endpoint_to_player_id_.end()) {
            // Existing player, update name if changed
            player_id = it->second;
        } else {
            // New player, create entity
            player_id = next_player_id_++;
            endpoint_to_player_id_[sender] = player_id;

            // Create player entity
            createPlayerEntity(player_id, payload.name);
        }

        // Send HelloAck
        sendHelloAck(sender);

        // Broadcast updated roster
        broadcastRoster();
    }
};
```

## Validation

### Client Validation (Before Sending)

**Required:**
- [ ] Token is valid (non-zero, received from TCP)
- [ ] Player name is valid UTF-8
- [ ] Player name length ≤ 15 characters
- [ ] Name is null-terminated within 16 bytes

**Example Validation:**
```cpp
// Validate token
if (token == 0) {
    throw std::invalid_argument("Invalid token");
}

// Validate name
if (player_name.size() > 15) {
    player_name = player_name.substr(0, 15);  // Truncate
}

// Validate UTF-8 (simplified check)
if (!is_valid_utf8(player_name)) {
    player_name = "Player";  // Fallback to default
}
```

### Server Validation (After Receiving)

**Required:**
- [ ] `header.version == 1`
- [ ] `header.type == 1` (Hello)
- [ ] `header.size == 20`
- [ ] `payload.token` matches expected token
- [ ] `payload.name` is null-terminated within 16 bytes

**Example Validation:**
```cpp
// Version check
if (header.version != ProtocolVersion) {
    return;  // Silently ignore
}

// Size check
if (header.size != sizeof(UdpHelloPayload)) {
    return;  // Invalid payload size
}

// Token validation
if (!isValidToken(payload.token, sender)) {
    return;  // Invalid token
}

// Name validation (ensure null termination)
payload.name[15] = '\0';  // Force null at max position

// Sanitize name (remove non-printable characters)
sanitizeName(payload.name);
```

## Wire Format Example

### Example 1: Token 0x12345678, Name "Alice"

**Complete Message:**
```
Offset  Hex                                        ASCII     Description
------  ------------------------------------------  --------  -----------
0x0000  14 00 01 01                                ....      Header (size=20, type=1, version=1)
0x0004  78 56 34 12                                xV4.      token=0x12345678
0x0008  41 6C 69 63 65 00 00 00 00 00 00 00 00 00  Alice....  name="Alice\0" (zero-padded)
0x0016  00 00                                      ..        (continued padding)
```

**Total Size:** 24 bytes

### Example 2: Token 0xDEADBEEF, Name "Bob"

**Complete Message:**
```
Offset  Hex                                        ASCII     Description
------  ------------------------------------------  --------  -----------
0x0000  14 00 01 01                                ....      Header
0x0004  EF BE AD DE                                ....      token=0xDEADBEEF
0x0008  42 6F 62 00 00 00 00 00 00 00 00 00 00 00  Bob.....  name="Bob\0"
0x0016  00 00                                      ..        (padding)
```

## Common Issues

### Issue 1: Server Doesn't Respond

**Problem:** Client sends Hello but never receives HelloAck

**Possible Causes:**
1. **Invalid Token:** Token doesn't match server's expected value
2. **Firewall:** UDP traffic blocked
3. **Wrong Port:** Client sending to wrong UDP port
4. **Server Not Running:** Server UDP socket not listening

**Debug Steps:**
```bash
# Verify token matches (add logging on both sides)
Client: "Sending token: 0x12345678"
Server: "Expected token: 0x12345678" (or "Invalid token received")

# Verify UDP connectivity
sudo tcpdump -i any udp port 4242 -X

# Check if packet reaches server
# Look for 24-byte UDP packet starting with: 14 00 01 01
```

### Issue 2: Name Truncation Issues

**Problem:** Long names cause parse errors or display incorrectly

**Cause:** Name not properly null-terminated or truncated

**Solution:**
```cpp
// Client-side: Always truncate and null-terminate
std::string truncated = player_name.substr(0, 15);
strncpy(payload.name, truncated.c_str(), 15);
payload.name[15] = '\0';  // Guarantee null termination

// Server-side: Force null termination
payload.name[15] = '\0';
```

### Issue 3: UTF-8 Display Issues

**Problem:** Names with special characters display as garbage

**Causes:**
1. Client sends invalid UTF-8
2. Server or other clients can't handle UTF-8

**Mitigation:**
```cpp
// Client: Validate UTF-8 before sending
if (!is_valid_utf8(name)) {
    name = sanitize_to_ascii(name);
}

// Server: Replace invalid UTF-8 with '?'
std::string sanitize(const char* name) {
    std::string result;
    for (size_t i = 0; i < 16 && name[i] != '\0'; ++i) {
        if (isprint(name[i]) || (name[i] & 0x80)) {  // ASCII printable or UTF-8 continuation
            result += name[i];
        } else {
            result += '?';
        }
    }
    return result;
}
```

## Security Considerations

### Token Security

**Current Level:** ⚠️ **INSECURE**

**Vulnerabilities:**
1. **Token Interception:** Token sent over plaintext TCP, can be captured
2. **Token Reuse:** Same token can be used multiple times
3. **No Expiration:** Token valid until server restart
4. **No IP Binding:** Token can be used from different IP

**Attack Scenario:**
```
1. Attacker sniffs TCP connection, captures token
2. Attacker sends UDP Hello with captured token
3. Server accepts attacker as legitimate player
4. Attacker can inject inputs, disrupt game
```

**Hardening:**
1. **Bind Token to IP:** Server validates UDP source IP matches TCP source IP
2. **One-Time Token:** Invalidate token after first use
3. **Expiration:** Token expires after N seconds
4. **HMAC:** Use HMAC with secret key instead of simple uint32

### Name Injection

**Risk:** Malicious client sends name with control characters or exploits

**Mitigations:**
- Validate UTF-8 encoding
- Strip control characters (0x00-0x1F except null terminator)
- Limit to printable characters
- HTML-escape if displaying in web UI

**Example:**
```cpp
bool isValidName(const char* name) {
    for (size_t i = 0; i < 15 && name[i] != '\0'; ++i) {
        if (name[i] < 0x20 && name[i] != '\0') {  // Control character
            return false;
        }
    }
    return true;
}
```

## Performance Characteristics

### Message Size
- **24 bytes** per client connection
- Sent once per session
- Negligible bandwidth impact

### Latency
- Single UDP datagram (no TCP handshake delay)
- Typical RTT: network latency + server processing (<1ms)
- May require retransmit if packet lost (adds RTT)

## Testing

### Unit Test Example

```cpp
TEST(Protocol, UdpHello_Serialization) {
    Header header{sizeof(UdpHelloPayload), MsgType::Hello, ProtocolVersion};

    UdpHelloPayload payload{};
    payload.token = 0x12345678;
    std::strcpy(payload.name, "Alice");

    uint8_t buffer[24];
    std::memcpy(buffer, &header, 4);
    std::memcpy(buffer + 4, &payload, 20);

    // Verify header
    EXPECT_EQ(buffer[0], 0x14);  // size = 20
    EXPECT_EQ(buffer[1], 0x00);
    EXPECT_EQ(buffer[2], 1);     // type = Hello
    EXPECT_EQ(buffer[3], 1);     // version

    // Verify token
    EXPECT_EQ(buffer[4], 0x78);
    EXPECT_EQ(buffer[5], 0x56);
    EXPECT_EQ(buffer[6], 0x34);
    EXPECT_EQ(buffer[7], 0x12);

    // Verify name
    EXPECT_STREQ(reinterpret_cast<char*>(buffer + 8), "Alice");
}

TEST(Protocol, UdpHello_NameTruncation) {
    std::string long_name = "ThisIsAVeryLongPlayerName";

    UdpHelloPayload payload{};
    payload.token = 123;

    // Truncate to 15 chars
    size_t len = std::min(long_name.size(), size_t(15));
    std::memcpy(payload.name, long_name.c_str(), len);
    payload.name[15] = '\0';

    // Verify truncation and null termination
    EXPECT_EQ(std::strlen(payload.name), 15);
    EXPECT_EQ(payload.name[15], '\0');
}
```

### Integration Test

```bash
# Send Hello with netcat won't work (binary protocol)
# Use custom test client or packet injection tool

# Example with Python and scapy
python3 << EOF
from scapy.all import *
import struct

# Build packet
header = struct.pack('<HBB', 20, 1, 1)  # size=20, type=1, version=1
token = struct.pack('<I', 0x12345678)
name = b'Alice' + b'\x00' * 11  # 16 bytes total

packet = header + token + name
send(IP(dst="127.0.0.1")/UDP(dport=4242)/Raw(load=packet))
EOF
```

## Retry Logic

### Recommended Client Implementation

```cpp
const int MAX_RETRIES = 3;
const auto RETRY_TIMEOUT = std::chrono::seconds(5);

bool connectUdp(uint32_t token, const std::string& name) {
    for (int attempt = 0; attempt < MAX_RETRIES; ++attempt) {
        // Send Hello
        sendUdpHello(socket_, server_endpoint_, token, name);

        // Wait for HelloAck
        auto deadline = std::chrono::steady_clock::now() + RETRY_TIMEOUT;

        while (std::chrono::steady_clock::now() < deadline) {
            if (receiveMessage()) {  // Non-blocking receive
                if (last_message_type_ == MsgType::HelloAck) {
                    return true;  // Success!
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        // Timeout, retry
        std::cout << "Hello timeout, retry " << (attempt + 1) << "\n";
    }

    return false;  // Failed after all retries
}
```

## Related Documentation

- **[tcp-100-welcome.md](tcp-100-welcome.md)** - Provides token for this message
- **[udp-02-hello-ack.md](udp-02-hello-ack.md)** - Server response to Hello
- **[03-data-structures.md](03-data-structures.md)** - UdpHelloPayload definition
- **[02-transport.md](02-transport.md)** - UDP transport details

---

**Last Updated:** 2025-10-29

