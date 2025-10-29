# Message Header

## Overview

Every message in the R-Type protocol, whether TCP or UDP, begins with a common 4-byte header. This header provides the message type, payload size, and protocol version.

## Structure Definition

### C++ Definition

```cpp
struct Header {
    std::uint16_t size;   // payload size excluding header
    MsgType type;         // uint8_t enum value
    std::uint8_t version; // protocol version
};
```

**Source:** `common/include/common/Protocol.hpp`

### Binary Layout

| Offset | Size | Type | Field | Description |
|--------|------|------|-------|-------------|
| 0 | 2 bytes | `uint16_t` | `size` | Payload size in bytes (excludes header itself) |
| 2 | 1 byte | `uint8_t` | `type` | Message type (see MsgType enum) |
| 3 | 1 byte | `uint8_t` | `version` | Protocol version (must be 1) |

**Total Header Size:** 4 bytes

### Memory Layout Notes

**Struct Packing:**
- The `Header` struct is **NOT** marked with `#pragma pack(push, 1)`
- Expected to be 4 bytes on all target platforms (x86, x86-64, ARM)
- No padding expected due to natural alignment

**Portability Warning:**
- Server and client **must** compile with the same ABI
- Different compilers or platforms may add padding differently
- If portability is required, add `#pragma pack(push, 1)` around `Header`

## Field Specifications

### size (uint16_t)

**Purpose:** Indicates the number of bytes in the message payload, **excluding** the 4-byte header itself.

**Range:** 0 to 65,535 bytes

**Endianness:** Little-endian (native)

**Examples:**
- Hello message: `size = 0` (no payload)
- Input message: `size = 5` (InputPacket is 5 bytes)
- State message: `size = 2 + (25 × entity_count)`

**Validation:**

Server currently **does not** strictly validate that `header.size` matches the actual received payload size. This is a potential security issue.

**Recommended Client Validation:**
```cpp
if (received_bytes < HeaderSize) {
    // Discard: packet too small
    return;
}

uint16_t expected_payload = header.size;
uint16_t actual_payload = received_bytes - HeaderSize;

if (actual_payload < expected_payload) {
    // Discard: truncated packet
    return;
}

// Proceed with parsing (ignore any extra bytes)
```

### type (MsgType / uint8_t)

**Purpose:** Identifies the message type for routing and parsing.

**Type:** Enum `MsgType` stored as `uint8_t`

**Value Ranges:**
- 1-99: UDP messages
- 100-199: TCP messages
- 200+: Reserved for future use

**Complete List:**

| Value | Name | Transport | Status |
|-------|------|-----------|--------|
| 1 | `Hello` | UDP | Active |
| 2 | `HelloAck` | UDP | Active |
| 3 | `Input` | UDP | Active |
| 4 | `State` | UDP | Active |
| 5 | `Spawn` | UDP | Reserved |
| 6 | `Despawn` | UDP | Reserved |
| 7 | `Ping` | UDP | Reserved |
| 8 | `Pong` | UDP | Reserved |
| 9 | `Roster` | UDP | Active |
| 10 | `LivesUpdate` | UDP | Active |
| 11 | `ScoreUpdate` | UDP | Active |
| 12 | `Disconnect` | UDP | Active |
| 13 | `ReturnToMenu` | UDP | Active |
| 100 | `TcpWelcome` | TCP | Active |
| 101 | `StartGame` | TCP | Active |

**Unknown Types:**

Both server and client should **silently ignore** messages with unknown type values. This allows for forward compatibility with newer protocol versions.

### version (uint8_t)

**Purpose:** Protocol version for compatibility checking.

**Current Version:** 1

**Validation:**

The server **rejects** all messages where `version != ProtocolVersion`:
```cpp
static constexpr std::uint8_t ProtocolVersion = 1;

// In handlePacket:
if (header->version != ProtocolVersion) {
    // Silently ignore
    return;
}
```

**Client Behavior:**

Clients should similarly validate the version:
```cpp
if (header.version != 1) {
    // Log warning, ignore packet
    return;
}
```

**Future Versions:**

When breaking changes are made to the protocol:
1. Increment `ProtocolVersion`
2. Implement version-specific parsing logic
3. Consider supporting multiple versions during transition period

## Constants

### C++ Constants

```cpp
namespace rtype::net {
    static constexpr std::uint8_t ProtocolVersion = 1;
    static constexpr std::size_t HeaderSize = sizeof(Header); // 4
}
```

**Source:** `common/include/common/Protocol.hpp`

## Binary Examples

### Example 1: Hello Message

**Hex Dump:**
```
00 00 01 01
```

**Parsed:**
- `size = 0x0000` (0): No payload
- `type = 0x01` (1): Hello
- `version = 0x01` (1): Protocol version 1

**Total Message Size:** 4 bytes (header only)

### Example 2: Input Message

**Hex Dump:**
```
05 00 03 01 00 00 00 00 08
```

**Parsed:**
- `size = 0x0005` (5): 5-byte payload
- `type = 0x03` (3): Input
- `version = 0x01` (1): Protocol version 1
- *(Followed by 5-byte InputPacket payload)*

**Total Message Size:** 9 bytes (4-byte header + 5-byte payload)

### Example 3: State Message (2 entities)

**Hex Dump:**
```
34 00 04 01 02 00 [... 50 bytes of entity data ...]
```

**Parsed:**
- `size = 0x0034` (52): 52-byte payload
- `type = 0x04` (4): State
- `version = 0x01` (1): Protocol version 1
- *Payload: StateHeader (2 bytes) + 2 × PackedEntity (25 bytes each) = 52 bytes*

**Total Message Size:** 56 bytes

## Parsing Code Examples

### C++ Parsing

```cpp
#include "common/Protocol.hpp"
#include <cstring>

void parseMessage(const uint8_t* buffer, size_t length) {
    using namespace rtype::net;

    // Validate minimum size
    if (length < HeaderSize) {
        // Discard: too small
        return;
    }

    // Parse header
    Header header;
    std::memcpy(&header, buffer, HeaderSize);

    // Validate version
    if (header.version != ProtocolVersion) {
        // Discard: wrong version
        return;
    }

    // Validate payload size
    size_t payload_size = length - HeaderSize;
    if (payload_size < header.size) {
        // Discard: truncated
        return;
    }

    // Route based on type
    const uint8_t* payload = buffer + HeaderSize;

    switch (header.type) {
        case MsgType::Hello:
            handleHello();
            break;
        case MsgType::Input:
            if (header.size >= sizeof(InputPacket)) {
                InputPacket input;
                std::memcpy(&input, payload, sizeof(input));
                handleInput(input);
            }
            break;
        case MsgType::State:
            handleState(payload, header.size);
            break;
        // ... other types
        default:
            // Unknown type, ignore
            break;
    }
}
```

### Python Parsing (Example)

```python
import struct

HEADER_SIZE = 4
PROTOCOL_VERSION = 1

def parse_message(data: bytes) -> tuple:
    """Parse R-Type message header.

    Returns: (size, type, version, payload) or None if invalid
    """
    if len(data) < HEADER_SIZE:
        return None

    # Unpack header (little-endian)
    size, msg_type, version = struct.unpack('<HBB', data[:HEADER_SIZE])

    # Validate version
    if version != PROTOCOL_VERSION:
        return None

    # Extract payload
    payload = data[HEADER_SIZE:]

    # Validate payload size
    if len(payload) < size:
        return None  # Truncated

    return (size, msg_type, version, payload[:size])
```

## Wire Format Notes

### Endianness

**Important:** The `size` field is 16 bits and stored in **little-endian** format.

**Example:**
- `size = 1234` (0x04D2)
- Wire format: `D2 04` (low byte first)

**No Conversion:** The protocol does **not** use network byte order (big-endian). This is intentional for performance but limits portability.

### Alignment

The header has no padding bytes. All fields are naturally aligned:
- `size` (2 bytes) at offset 0: naturally aligned
- `type` (1 byte) at offset 2: naturally aligned
- `version` (1 byte) at offset 3: naturally aligned

## Common Mistakes

### Mistake 1: Including Header in Size

**Wrong:**
```cpp
header.size = sizeof(Header) + sizeof(payload);  // ❌ DON'T include header
```

**Correct:**
```cpp
header.size = sizeof(payload);  // ✅ Payload size only
```

### Mistake 2: Big-Endian Conversion

**Wrong:**
```cpp
header.size = htons(payload_size);  // ❌ Don't use network byte order
```

**Correct:**
```cpp
header.size = payload_size;  // ✅ Use native (little-endian)
```

### Mistake 3: Not Validating Version

**Wrong:**
```cpp
// Parse all messages regardless of version  // ❌ May break on future changes
```

**Correct:**
```cpp
if (header.version != ProtocolVersion) {
    return;  // ✅ Reject wrong version
}
```

## Performance Considerations

### Header Overhead

- **4 bytes per message**
- At 60 Hz: 240 bytes/sec per message type
- For Input + State: 480 bytes/sec overhead = 3.8 Kbps

This is negligible compared to payload sizes, especially for State messages.

### Parsing Performance

The header is designed for fast parsing:
- No bit fields (byte-aligned)
- No variable-length fields
- Simple type dispatch via switch statement
- Can be parsed with a single 32-bit read on little-endian systems

## Security Considerations

### Validation Required

**Minimal Validation (Current Server):**
1. Check `length >= HeaderSize`
2. Check `version == ProtocolVersion`
3. Route by `type`

**Recommended Validation (Production):**
1. Check `length >= HeaderSize`
2. Check `version == ProtocolVersion`
3. Check `header.size == (length - HeaderSize)` (strict match)
4. Check `type` is in valid range for transport (TCP vs UDP)
5. Rate limit: track messages per source IP per second

### Attack Vectors

**Oversized Size Field:**
- Attacker sends `size = 65535` but only 100 bytes payload
- Current server: may read beyond buffer if not careful
- Mitigation: Validate actual received length

**Zero Size with Payload:**
- Attacker sends `size = 0` with non-zero payload
- Current server: ignores payload (safe)
- Mitigation: None needed (safe to ignore)

**Version Mismatch:**
- Attacker sends random version numbers
- Current server: silently ignores (safe)
- Mitigation: None needed (already handled)

## Related Documentation

- **[00-overview.md](00-overview.md)** - Protocol fundamentals
- **[02-transport.md](02-transport.md)** - TCP vs UDP usage
- **[03-data-structures.md](03-data-structures.md)** - Payload structures

---

**Last Updated:** 2025-10-29

