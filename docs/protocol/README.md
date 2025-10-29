# R-Type Network Protocol Documentation

## Overview

This directory contains the complete specification of the R-Type network protocol. The protocol is designed for real-time multiplayer gameplay using a client-server architecture with both TCP (for initial connection) and UDP (for gameplay) transports.

**Protocol Version:** 1
**Default Server Port (UDP):** 4242
**Transport:** TCP for handshake, UDP for gameplay
**Endianness:** Little-endian (native, no network byte order conversion)
**Encryption:** None (plaintext)

## Documentation Structure

### Core Protocol Files

- **[00-overview.md](00-overview.md)** - Protocol fundamentals, transport layer, versioning
- **[01-header.md](01-header.md)** - Common message header structure
- **[02-transport.md](02-transport.md)** - TCP vs UDP usage, connection flow
- **[03-data-structures.md](03-data-structures.md)** - Common data structures and types

### Message Type Documentation

Each message type has its own detailed specification file:

#### TCP Messages (100+)
- **[tcp-100-welcome.md](tcp-100-welcome.md)** - `TcpWelcome` - Initial TCP connection
- **[tcp-101-start-game.md](tcp-101-start-game.md)** - `StartGame` - Game start signal

#### UDP Messages (1-99)

##### Connection & Session (1-2)
- **[udp-01-hello.md](udp-01-hello.md)** - `Hello` - UDP handshake with token
- **[udp-02-hello-ack.md](udp-02-hello-ack.md)** - `HelloAck` - UDP handshake acknowledgment

##### Gameplay Core (3-4)
- **[udp-03-input.md](udp-03-input.md)** - `Input` - Client input commands
- **[udp-04-state.md](udp-04-state.md)** - `State` - World state synchronization

##### Entity Management (5-6)
- **[udp-05-spawn.md](udp-05-spawn.md)** - `Spawn` - Entity spawn events (reserved)
- **[udp-06-despawn.md](udp-06-despawn.md)** - `Despawn` - Entity removal events (reserved)

##### Player Management (9-12)
- **[udp-09-roster.md](udp-09-roster.md)** - `Roster` - Player list with names and lives
- **[udp-10-lives-update.md](udp-10-lives-update.md)** - `LivesUpdate` - Player lives changed
- **[udp-11-score-update.md](udp-11-score-update.md)** - `ScoreUpdate` - Player score changed
- **[udp-12-disconnect.md](udp-12-disconnect.md)** - `Disconnect` - Client disconnect notification

##### Session Control (13)
- **[udp-13-return-to-menu.md](udp-13-return-to-menu.md)** - `ReturnToMenu` - Server requests client return to menu

## Quick Reference

### Message Type Values

| Type | Value | Direction | Transport | Status |
|------|-------|-----------|-----------|--------|
| `Hello` | 1 | Client → Server | UDP | Active |
| `HelloAck` | 2 | Server → Client | UDP | Active |
| `Input` | 3 | Client → Server | UDP | Active |
| `State` | 4 | Server → Client | UDP | Active |
| `Spawn` | 5 | Server → Client | UDP | Reserved |
| `Despawn` | 6 | Server → Client | UDP | Reserved |
| `Roster` | 9 | Server → Client | UDP | Active |
| `LivesUpdate` | 10 | Server → Client | UDP | Active |
| `ScoreUpdate` | 11 | Server → Client | UDP | Active |
| `Disconnect` | 12 | Client → Server | UDP | Active |
| `ReturnToMenu` | 13 | Server → Client | UDP | Active |
| `TcpWelcome` | 100 | Server → Client | TCP | Active |
| `StartGame` | 101 | Server → Client | TCP | Active |

### Server Timing

- **Tick Rate:** 60 Hz (~16.66ms per tick)
- **State Broadcast:** ~60 Hz to all connected clients
- **Enemy Spawn:** Every ~2 seconds
- **Input Processing:** Latest received input per player per tick

### Size Constraints

- **Header Size:** 4 bytes
- **Max State Entities:** 512 entities (can cause UDP fragmentation)
- **Max Username:** 15 characters + null terminator (16 bytes)
- **Receive Buffer:** 2048 bytes (server)

## Implementation Notes

### Critical Constraints

1. **No Endianness Conversion:** All data is sent in native little-endian format. Clients must use compatible architectures.
2. **No Reliability:** UDP messages may be lost, duplicated, or reordered. Client implementations must handle this.
3. **No Authentication:** Current protocol has no built-in authentication or encryption.
4. **Struct Packing:** All payload structures use `#pragma pack(push, 1)` to ensure consistent binary layout.

### Security Considerations

⚠️ **WARNING:** This protocol is designed for trusted networks and educational purposes.

Current limitations:
- No authentication of clients
- No rate limiting or flood protection
- No encryption of data
- Vulnerable to spoofing and man-in-the-middle attacks
- Header size field not strictly validated

See individual message files for specific security notes.

## Client Implementation Checklist

When implementing a client, ensure you:

- [ ] Establish TCP connection first to get UDP port and token
- [ ] Send UDP `Hello` with token before gameplay
- [ ] Validate received message headers (version, size)
- [ ] Handle UDP packet loss gracefully (apply latest state)
- [ ] Limit input transmission rate (30-60 Hz recommended)
- [ ] Implement timeout for server unresponsiveness
- [ ] Parse `State` messages carefully (validate entity count)
- [ ] Handle `ReturnToMenu` message for graceful game end

## Code References

- **Protocol Definition:** `common/include/common/Protocol.hpp`
- **Server Implementation:** `server/UdpServer.cpp`, `server/UdpServer.hpp`
- **Server Entry Point:** `server/main.cpp`

## Extensions and Future Work

The protocol includes reserved message types for future features:

- **Spawn/Despawn:** Delta-based entity updates (more efficient than full state)
- **Input Sequence:** Currently sent but not used for input validation

## Version History

- **Version 1 (Current):** Initial protocol with TCP handshake and UDP gameplay

---

**Document Version:** 1.0
**Last Updated:** 2025-10-29
**Maintained By:** R-Type Development Team

