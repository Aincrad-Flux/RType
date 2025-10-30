# TcpWelcome (100)

## Overview

**Message Type:** `TcpWelcome` (100)
**Transport:** TCP
**Direction:** Server → Client
**Purpose:** Initial message sent by server after TCP connection, provides UDP port number
**Status:** Active

## Message Format

### Complete Message Structure

```
┌─────────────────────────┬───────────────────────┐
│   Header (4 bytes)      │  Payload (6 bytes)    │
├─────────────────────────┼───────────────────────┤
│ size=6, type=100, ver=1 │  HelloAckPayload      │
└─────────────────────────┴───────────────────────┘
```

**Total Message Size:** 10 bytes

### Header

| Field | Value | Encoding |
|-------|-------|----------|
| `size` | 6 | `uint16_t` (little-endian) |
| `type` | 100 | `uint8_t` (TcpWelcome) |
| `version` | 1 | `uint8_t` |

**Wire Format (Header):**
```
06 00 64 01
└─ size=6
      └─ type=100 (0x64)
         └─ version=1
```

### Payload: HelloAckPayload

**Structure:**
```cpp
#pragma pack(push, 1)
struct HelloAckPayload {
    std::uint16_t udpPort;  // UDP port to use
    std::uint32_t token;    // Session token to present in UDP Hello
};
#pragma pack(pop)
```

**Size:** 6 bytes

**Binary Layout:**

| Offset | Size | Type | Field | Description |
|--------|------|------|-------|-------------|
| 0 | 2 bytes | `uint16_t` | `udpPort` | UDP port (little-endian) |
| 2 | 4 bytes | `uint32_t` | `token` | Authentication token (little-endian) |

## Field Specifications

### udpPort

**Type:** `uint16_t` (2 bytes, little-endian)

**Purpose:** Tells client which UDP port to use for gameplay traffic

**Typical Values:**
- Same as TCP port (e.g., 4242)
- Different port if server uses separate UDP port
- Dynamic port (if server binds to port 0 and gets OS-assigned port)

**Range:** 1-65535 (well-known ports 1-1023 typically avoided)

**Client Action:**
1. Parse `udpPort` from payload
2. Store for later use
3. Send UDP Hello to server IP at this port

**Example:**

UDP port 4242 (0x1092):
```
Wire: 92 10
      └─ 0x1092 = 4242 in little-endian
```

### token

**Type:** `uint32_t` (4 bytes, little-endian)

**Purpose:** Authentication token for UDP session establishment

**Token Generation:**
- Server-generated, arbitrary value
- Could be random, sequential, or derived from client info
- Unique per client connection (at least within active sessions)

**Security Properties:**
- **NOT cryptographically secure** in current implementation
- Vulnerable to interception (sent over plaintext TCP)
- Vulnerable to replay attacks (no expiration mechanism)

**Client Action:**
1. Parse `token` from payload
2. Store securely
3. Include in UDP `Hello` message
4. Server validates token before creating player

**Example:**

Token value 0x12345678:
```
Wire: 78 56 34 12
      └─ 0x12345678 in little-endian
```

## Protocol Flow

### Connection Sequence

```
Client                                Server
   │                                     │
   │───── TCP Connect ────────────────────>│
   │                                     │
   │                                     │ [Accept connection]
   │                                     │ [Generate token]
   │                                     │ [Prepare UDP port]
   │                                     │
   │<──── TcpWelcome (port, token) ───────│
   │                                     │
   │ [Parse port and token]              │
   │ [Prepare UDP socket]                │
   │                                     │
```

**Timing:**
- Server sends `TcpWelcome` immediately after accepting TCP connection
- Typically within milliseconds of connection establishment
- No client request needed (unsolicited message)

## Implementation Examples

### Server (Sending)

```cpp
#include "common/Protocol.hpp"
#include <asio.hpp>
#include <cstring>

using namespace rtype::net;

void sendTcpWelcome(asio::ip::tcp::socket& socket, uint16_t udp_port, uint32_t token) {
    // Prepare header
    Header header;
    header.size = sizeof(HelloAckPayload);
    header.type = MsgType::TcpWelcome;
    header.version = ProtocolVersion;

    // Prepare payload
    HelloAckPayload payload;
    payload.udpPort = udp_port;
    payload.token = token;

    // Combine into single buffer
    uint8_t buffer[sizeof(Header) + sizeof(HelloAckPayload)];
    std::memcpy(buffer, &header, sizeof(Header));
    std::memcpy(buffer + sizeof(Header), &payload, sizeof(HelloAckPayload));

    // Send (blocking)
    asio::write(socket, asio::buffer(buffer, sizeof(buffer)));
}
```

### Client (Receiving)

```cpp
#include "common/Protocol.hpp"
#include <asio.hpp>
#include <cstring>

using namespace rtype::net;

struct WelcomeInfo {
    uint16_t udpPort;
    uint32_t token;
};

std::optional<WelcomeInfo> receiveTcpWelcome(asio::ip::tcp::socket& socket) {
    // Read header
    Header header;
    asio::read(socket, asio::buffer(&header, sizeof(Header)));

    // Validate
    if (header.version != ProtocolVersion) {
        return std::nullopt;  // Wrong version
    }

    if (header.type != MsgType::TcpWelcome) {
        return std::nullopt;  // Unexpected message type
    }

    if (header.size != sizeof(HelloAckPayload)) {
        return std::nullopt;  // Wrong payload size
    }

    // Read payload
    HelloAckPayload payload;
    asio::read(socket, asio::buffer(&payload, sizeof(HelloAckPayload)));

    // Return parsed data
    WelcomeInfo info;
    info.udpPort = payload.udpPort;
    info.token = payload.token;
    return info;
}
```

## Validation

### Server Validation (Before Sending)

✅ **No validation needed** - server controls all values

**Best Practices:**
- Ensure `udpPort` is the actual port UDP socket is bound to
- Generate unique token per connection
- Log token generation for debugging

### Client Validation (After Receiving)

**Required:**
- [ ] `header.version == 1`
- [ ] `header.type == 100` (TcpWelcome)
- [ ] `header.size == 6`
- [ ] `payload.udpPort >= 1 && payload.udpPort <= 65535`
- [ ] `payload.token != 0` (optional, depending on server token generation)

**Error Handling:**

```cpp
// Version mismatch
if (header.version != ProtocolVersion) {
    // Log error, disconnect, show "Protocol version mismatch"
    return Error::VersionMismatch;
}

// Invalid port
if (payload.udpPort == 0) {
    // Log error, disconnect, show "Invalid UDP port"
    return Error::InvalidPort;
}

// Token validation (if server guarantees non-zero)
if (payload.token == 0) {
    // Log warning, may still be valid depending on implementation
}
```

## Wire Format Example

### Example 1: Port 4242, Token 0x12345678

**Complete Message:**
```
Offset  Hex                      ASCII       Description
------  ----------------------   -------     -----------
0x0000  06 00 64 01              ....        Header (size=6, type=100, version=1)
0x0004  92 10                    ..          udpPort=4242 (0x1092)
0x0006  78 56 34 12              xV4.        token=0x12345678
```

**Breakdown:**
- `06 00`: size = 6
- `64`: type = 100 (0x64 = TcpWelcome)
- `01`: version = 1
- `92 10`: udpPort = 4242 (0x10 0x92 in little-endian = 0x1092 = 4242)
- `78 56 34 12`: token = 0x12345678 (little-endian)

### Example 2: Port 5000, Token 0xDEADBEEF

**Complete Message:**
```
Offset  Hex                      ASCII       Description
------  ----------------------   -------     -----------
0x0000  06 00 64 01              ....        Header
0x0004  88 13                    ..          udpPort=5000 (0x1388)
0x0006  EF BE AD DE              ....        token=0xDEADBEEF
```

## Common Issues

### Issue 1: Wrong Endianness

**Problem:** Client interprets port 4242 (0x1092) as 37392 (0x9210)

**Cause:** Client code uses big-endian or doesn't convert correctly

**Solution:** Ensure no byte swapping; use native little-endian:
```cpp
// WRONG
uint16_t port = ntohs(payload.udpPort);  // Don't use ntohs!

// CORRECT
uint16_t port = payload.udpPort;  // Use directly (little-endian)
```

### Issue 2: Client Can't Connect to UDP Port

**Problem:** Client sends UDP Hello but server doesn't respond

**Possible Causes:**
1. Server UDP socket not bound to the port sent in TcpWelcome
2. Firewall blocking UDP traffic
3. Client sending to wrong IP address (sent UDP to different IP than TCP)

**Debug Steps:**
```bash
# Verify server is listening on UDP port
netstat -an | grep 4242

# Capture UDP traffic
sudo tcpdump -i any udp port 4242 -X

# Test with netcat (note: nc doesn't support UDP protocol messages, but tests connectivity)
nc -u server_ip 4242
```

### Issue 3: Token Mismatch

**Problem:** Server rejects UDP Hello due to invalid token

**Causes:**
1. Client didn't preserve token correctly (truncation, overflow)
2. Server regenerated token or forgot client's token
3. Token expired (if server has expiration logic)
4. Client sent to wrong server instance (load balancer, multiple servers)

**Solution:**
- Log token on both client and server
- Ensure uint32_t storage on client
- Verify endianness conversion if any

## Security Considerations

### Current Security Level: ⚠️ INSECURE

**Vulnerabilities:**

1. **No Encryption:**
   - TCP connection is plaintext
   - Token visible to network sniffers
   - Attacker can capture token and impersonate client

2. **No Token Validation:**
   - Token is arbitrary uint32 (possibly sequential or random)
   - No cryptographic integrity (HMAC, signature)
   - Vulnerable to brute force if token space is small

3. **No Expiration:**
   - Token valid indefinitely
   - If captured, can be replayed anytime server is running

4. **No Client Identification:**
   - Server can't verify token came from same TCP connection
   - Attacker can use token from different IP

### Recommended Hardening

**Phase 1: Immediate (Minimal Changes)**
1. Use cryptographically strong random tokens (128+ bits)
2. Add token expiration (e.g., 60 seconds)
3. Validate UDP Hello comes from same IP as TCP connection

**Phase 2: Moderate**
1. Use TLS for TCP connection (encrypt token)
2. Include IP address in token generation (bind token to source)
3. Add rate limiting for failed token validations

**Phase 3: Full Security**
1. Replace token with HMAC or JWT
2. Use DTLS for UDP (encrypted gameplay)
3. Implement proper session management with refresh tokens

## Performance Characteristics

### Message Size
- **10 bytes** per client connection
- Sent once per client (negligible bandwidth impact)

### Latency
- Adds one TCP round-trip to connection establishment
- Typical time: 1-100ms depending on network
- Blocking: Client must wait for TcpWelcome before proceeding to UDP

### Server Load
- Token generation: O(1), <1μs (if using simple counter)
- Token generation: O(n), ~10μs (if using crypto RNG)
- Negligible CPU impact

## Testing

### Manual Testing with Netcat

**Not directly possible** - netcat can't parse binary protocol

Use custom test client:
```cpp
// Connect to server
asio::ip::tcp::socket socket(io_context);
socket.connect(tcp::endpoint(address::from_string("127.0.0.1"), 4242));

// Receive TcpWelcome
auto welcome = receiveTcpWelcome(socket);
if (welcome) {
    std::cout << "UDP Port: " << welcome->udpPort << "\n";
    std::cout << "Token: 0x" << std::hex << welcome->token << "\n";
}
```

### Packet Capture

```bash
# Capture TCP on port 4242
sudo tcpdump -i any tcp port 4242 -X -s 0

# Look for:
# - 10-byte message after TCP handshake
# - Starts with: 06 00 64 01
```

### Unit Test Example

```cpp
TEST(Protocol, TcpWelcome_Serialization) {
    Header header{6, MsgType::TcpWelcome, ProtocolVersion};
    HelloAckPayload payload{4242, 0x12345678};

    uint8_t buffer[10];
    std::memcpy(buffer, &header, 4);
    std::memcpy(buffer + 4, &payload, 6);

    // Verify wire format
    EXPECT_EQ(buffer[0], 0x06);  // size low byte
    EXPECT_EQ(buffer[1], 0x00);  // size high byte
    EXPECT_EQ(buffer[2], 100);   // type
    EXPECT_EQ(buffer[3], 1);     // version
    EXPECT_EQ(buffer[4], 0x92);  // port low byte (4242 = 0x1092)
    EXPECT_EQ(buffer[5], 0x10);  // port high byte
    EXPECT_EQ(buffer[6], 0x78);  // token bytes
    EXPECT_EQ(buffer[7], 0x56);
    EXPECT_EQ(buffer[8], 0x34);
    EXPECT_EQ(buffer[9], 0x12);
}
```

## Related Documentation

- **[02-transport.md](02-transport.md)** - TCP/UDP transport layer
- **[tcp-101-start-game.md](tcp-101-start-game.md)** - Next TCP message (StartGame)
- **[udp-01-hello.md](udp-01-hello.md)** - UDP Hello (uses token from this message)
- **[03-data-structures.md](03-data-structures.md)** - HelloAckPayload definition

## Version History

- **Version 1.0:** Initial implementation

---

**Last Updated:** 2025-10-29

