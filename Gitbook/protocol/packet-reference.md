---
icon: list
---

# Packet reference

A quick index of message types.

- Hello (1)
  - Direction: Client → Server
  - Payload: none
  - Purpose: start handshake

- HelloAck (2)
  - Direction: Server → Client
  - Payload: none
  - Purpose: confirm handshake

- Input (3)
  - Direction: Client → Server
  - Payload: `InputPacket{uint32 sequence, uint8 bits}`
  - Purpose: authoritative intent (movement/firing)

- State (4)
  - Direction: Server → Client
  - Payload: `StateHeader{uint16 count}` + `count * PackedEntity`
  - Purpose: world snapshot

- Spawn (5), Despawn (6)
  - Reserved for detailed deltas; currently unused

- Ping (7), Pong (8)
  - Reserved for RTT/keep-alive

See the specification for binary layouts.
