---
icon: binary
---

# Protocol specification (wire format)

This page describes the precise binary protocol between client and server.

## Overview

- Transport: UDP (IPv4)
- Default server port: 4242
- Binary, unencrypted, best-effort (no app-level retransmission)
- Endianness: little-endian native (no htons/ntohs)
- Protocol version: 1 (mismatched versions are ignored)
- MTU/fragmentation: not handled at application level; large snapshots may be fragmented by IP

## Cadence

- Server tick: ~60 Hz
- Server broadcasts State snapshots to all known clients each tick (budgeted)
- Clients send Input messages at a moderate rate (e.g., 30–60 Hz)

## Common header

C++ layout (not packed):

```
struct Header {
  std::uint16_t size;   // payload size, excluding header
  MsgType type;         // uint8
  std::uint8_t version; // uint8
};
```

- Expected size on targets: 4 bytes (no padding observed with current ABI)
- Constants: `ProtocolVersion = 1`, `HeaderSize = sizeof(Header)`
- Note: the server currently does not strictly validate `header.size` against the received payload length for all message types

## Message types (MsgType)

- 1: Hello — client handshake (no payload)
- 2: HelloAck — server handshake ack (no payload)
- 3: Input — client inputs (payload: `InputPacket`)
- 4: State — world snapshot (payload: `StateHeader` + N × `PackedEntity`)
- 5: Spawn — reserved (unused)
- 6: Despawn — reserved (unused)
- 7: Ping — reserved
- 8: Pong — reserved

## Hello / HelloAck

- Client → Server: `Header{size=0,type=Hello(1),version=1}`
- Server → Client: `Header{size=0,type=HelloAck(2),version=1}`

Effects on the server:
- Associates the UDP endpoint with a `playerId` (if new)
- Creates a Player entity (initial position and color)
- Initializes the player's input bits to 0

Example Hello (little-endian): `00 00 01 01`

## Input

Bitmask (bits in `bits` byte):
- Up:    1 << 0 (0x01)
- Down:  1 << 1 (0x02)
- Left:  1 << 2 (0x04)
- Right: 1 << 3 (0x08)
- Shoot: 1 << 4 (0x10)

Payload (packed, 1-byte alignment):

```
#pragma pack(push, 1)
struct InputPacket {
  std::uint32_t sequence; // client-side incrementing id (currently ignored)
  std::uint8_t  bits;     // input bitmask
};
#pragma pack(pop)
```

- Size: 5 bytes
- Server uses the latest `bits` per player when integrating movement

Example (sequence=42, Right+Shoot):
- Header: size=5 → `05 00`, type=3 → `03`, version=1 → `01`
- Payload: `2A 00 00 00 18`
- Total: `05 00 03 01 2A 00 00 00 18`

## State snapshot

Entity categories:
- Player (1)
- Enemy (2)
- Bullet (3)

Payload (packed):

```
#pragma pack(push, 1)
struct StateHeader { std::uint16_t count; };

struct PackedEntity {
  std::uint32_t id;
  std::uint8_t  type;  // 1=Player,2=Enemy,3=Bullet
  float x; float y;
  float vx; float vy;
  std::uint32_t rgba;  // 0xRRGGBBAA
};
#pragma pack(pop)
```

- `StateHeader`: 2 bytes
- `PackedEntity`: 25 bytes
- Payload size: `2 + count * 25`
- Server caps `count` (e.g., ≤ 512) and prioritizes players → bullets → enemies to fit a safe UDP budget

## Notes on binary representation

- All integers/floats sent in native little-endian
- Floats are IEEE-754 32-bit
- For strict network portability, consider explicit serialization and fixed endianness

## Validation and robustness

- On receive, check: `datagram.size() >= HeaderSize` and `header.version == ProtocolVersion`
- For `Input`, server checks `payloadSize >= sizeof(InputPacket)`
- Anti-flood, strict size checks, authentication, and checksums are not implemented yet

## Sizes and limits

- Header: 4 bytes
- InputPacket: 5 bytes
- StateHeader: 2 bytes
- PackedEntity: 25 bytes
- Snapshot upper bound example: with 512 entities, payload ≈ 12,802 bytes (risk of fragmentation)

## Code references

- Protocol definitions: `common/include/common/Protocol.hpp`
- Server I/O: `server/src/UdpServer.cpp`, `server/include/UdpServer.hpp`
- Server entry: `server/src/main.cpp`
