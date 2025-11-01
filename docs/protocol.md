# R-Type Network Protocol (Ultra Precise)

This document precisely describes the binary protocol used between the client and server for R-Type.
It covers: transport, versioning, headers, message types, binary formats, frequency, and important points.

## Overview

- Transport: UDP (asio, IPv4)
- Default server port: 4242 (configurable via CLI argument)
- Protocol: minimal binary, unencrypted, unreliable (no retransmission, no application-level ACKs, variable latency)
- Endianness: native little-endian (no conversion to "network byte order" is performed)
- Protocol version: 1 (packets with a different version are ignored on the server side)
- MTU/fragmentation: not explicitly handled. Large states may be fragmented by IP; no application-level reassembly.

## Cadence and Flow

- Server loop: 60 Hz (tick ~16.66 ms)
- State broadcasts (MsgType::State): approximately at each tick to all known clients
- Client inputs (MsgType::Input): free frequency; the server applies the last input received per player

## Common Message Header

C++ structure (not packed):

- size: uint16, payload size (excluding header)
- type: MsgType (uint8)
- version: uint8

Expected size: 4 bytes on target platforms (no padding observed with current ABI). There is no pragma pack here – server and client must compile with the same ABI to guarantee the same size/alignment.

Extract: `src/common/include/common/Protocol.hpp`

```
struct Header {
    std::uint16_t size;   // payload size excluding header
    MsgType type;         // uint8
    std::uint8_t version; // uint8
};
```

- Useful constants: `ProtocolVersion = 1`, `HeaderSize = sizeof(Header)`

Note: The server does not verify that `header->size` exactly matches the actual payload size received, except for `Input` where it checks `payloadSize >= sizeof(InputPacket)`.

## Message Types (MsgType)

- 1: Hello — client initialization handshake (empty payload)
- 2: HelloAck — server response to Hello (empty payload)
- 3: Input — client inputs (payload: `InputPacket`)
- 4: State — world state (payload: `StateHeader` + N × `PackedEntity`)
- 5: Spawn — reserved (not currently used)
- 6: Despawn — reserved (not currently used)
- 7: Ping — reserved (not currently used)
- 8: Pong — reserved (not currently used)

## Handshake (Hello / HelloAck)

- Client -> Server: `Header{ size=0, type=Hello(1), version=1 }`
- Server -> Client: `Header{ size=0, type=HelloAck(2), version=1 }`

Server effects:
- Associates the UDP endpoint with a unique `playerId` if new
- Creates a `Player` entity on the server (initial position x=50, y=100+offset, color RGBA 0x55AAFFFF)
- Initializes input bits to 0 for this player

Hex example (little-endian) for Hello: `00 00 01 01` (size=0x0000, type=0x01, version=0x01)

## Client Inputs (Input)

### Input Bitmask

- Up:   1 << 0 (0x01)
- Down: 1 << 1 (0x02)
- Left: 1 << 2 (0x04)
- Right:1 << 3 (0x08)
- Shoot:1 << 4 (0x10) — not currently handled by the server

### Payload: InputPacket (packed 1 byte)

```
#pragma pack(push, 1)
struct InputPacket {
    std::uint32_t sequence; // incrementing id on client side (currently ignored by the server)
    std::uint8_t bits;      // combination of input bits
};
#pragma pack(pop)
```

- Size: 5 bytes
- Endianness: `sequence` in little-endian (native)
- Server usage: ignores `sequence`, reads `bits` and updates `playerInputBits_[playerId]`

Example Input frame (sequence=42, Right+Shoot):
- Header: size=5 -> `05 00`, type=Input(3) -> `03`, version=1 -> `01`
- Payload: sequence=42 -> `2A 00 00 00`, bits=0x18 -> `18`
- Total hex: `05 00 03 01 2A 00 00 00 18`

### Server-Side Application

At each tick:
- For each `Player` entity, reads current bits
- Velocity: 150 px/s; calculates vx/vy according to bits; updates x/y with dt=1/60

## World State (State)

### Entity Types

- Player (1)
- Enemy (2)
- Bullet (3) — not currently emitted

### Payload: StateHeader + N × PackedEntity (packed 1 byte)

```
#pragma pack(push, 1)
struct StateHeader { std::uint16_t count; };

struct PackedEntity {
    std::uint32_t id;      // unique identifier
    EntityType type;       // uint8 (1=Player,2=Enemy,3=Bullet)
    float x; float y;      // position
    float vx; float vy;    // velocity
    std::uint32_t rgba;    // 0xRRGGBBAA
};
#pragma pack(pop)
```

- `StateHeader` size: 2 bytes
- `PackedEntity` size: 25 bytes (with pack(1))
- Server calculates: `payloadSize = 2 + count * 25`
- `count` limited to `min(entities.size, 512)` on server side
- Broadcast: to all known endpoints, at ~60 Hz

MTU note: With 512 entities, the payload ~12,802 bytes; `Header`=4; total ~12,806 bytes — likely to be fragmented by UDP.

## Enemy Spawns and Minimal Server Logic

- Spawn an enemy every ~2 s at x=900 (random y 20..500), vx=-60
- Remove enemies when x < -50
- Players move via inputs; no shooting or collisions implemented in this protocol

## Binary Representation and Endianness

- No `hton*/ntoh*` is used: all integers and floats are sent in machine representation (little-endian on target platforms)
- Floats: IEEE-754 32-bit in little-endian
- Implication: the client must be compiled/executed on a compatible little-endian architecture; for strict network portability, deterministic serialization would need to be introduced

## Server Reception and Validation

In `UdpServer::handlePacket`:
- Checks: size >= HeaderSize
- Checks: `header->version == ProtocolVersion`
- Routes according to `header->type`
- Hello: registers endpoint, creates player if necessary, returns HelloAck (Header only, size=0)
- Input: if `payloadSize >= sizeof(InputPacket)` then updates `playerInputBits_`
- Other types: ignored (no error log)

Points to note:
- `header->size` is not strictly validated vs `payloadSize`
- No anti-flood/rate limit
- No authentication or encryption

## Sequence Examples

1) Client Connection
- Client sends: Hello
- Server responds: HelloAck
- Server begins including the player in `State` messages

2) Active Gameplay
- Client regularly sends `Input` (for example at 60 Hz or on each change)
- Server broadcasts `State` (~60 Hz) to all clients

## Message Contract (Summary)

- Input -> Server
  - Header(type=Input, version=1, size=5) + InputPacket
  - Errors: version != 1 ignored; payload < 5 ignored

- State <- Server
  - Header(type=State, version=1, size=2+25*N) + StateHeader(count=N) + N*PackedEntity
  - N ≤ 512 (on server side)

- Handshake
  - Hello/HelloAck: Header only, size=0

## Client Recommendations (Implementation)

- Always verify:
  - Datagram size >= HeaderSize
  - `header.version == 1`
  - Optional: `header.size == receivedPayload`
- Parse `State` with attention to sizes: `payload >= 2 + 25*N`
- Handle packet loss/duplication (UDP): apply "last state received"; optionally interpolate/extrapolate on render side
- Limit `Input` send frequency (e.g., 30–60 Hz or on change)

## Extensions and Reserved Fields

- MsgType::Spawn / ::Despawn: intended for delta state; not implemented
- MsgType::Ping / ::Pong: intended for RTT/keep-alive; not implemented
- `InputPacket.sequence`: intended to disambiguate order; not currently used

## Security and Robustness

- No origin control: any endpoint that can send Hello receives state
- Risks: spoofing, flood, malformed payload
- Suggested hardening:
  - Verify `header.size`
  - Ignore/blacklist noisy endpoints
  - Heartbeat (Ping/Pong) and inactivity timeout
  - Explicit serialization (fixed endianness) and optional checksums

## Appendix: Sizes and Constants

- Header: 4 bytes
- InputPacket: 5 bytes
- StateHeader: 2 bytes
- PackedEntity: 25 bytes
- Max entities per `State`: 512
- Server receive buffer: 2048 bytes (for receiving Input/Hello)

## Code References

- Protocol definition: `src/common/include/common/Protocol.hpp`
- Server (send/receive): `src/server/UdpServer.cpp`, `src/server/UdpServer.hpp`
- Server launch (default port 4242): `src/server/main.cpp`