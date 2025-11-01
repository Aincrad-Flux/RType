---
icon: gamepad-2
---

# UDP Messages - Gameplay

Core gameplay messages exchanged during active gameplay.

## Message Types

| Type | Name | Direction | Purpose |
|------|------|-----------|---------|
| 3 | [Input](udp-03-input.md) | Client → Server | Player input commands (movement, shooting) |
| 4 | [State](udp-04-state.md) | Server → Client | Complete world state snapshot |

## Gameplay Loop

```
Client                    Server
  |                          |
  |-- Input (60 Hz) --------->|
  |                          |
  |              [Update world]|
  |              [60 Hz tick]  |
  |                          |
  |<-- State (60 Hz) ---------| (All entities)
  |                          |
  | [Render latest state]     |
  |                          |
```

## Message Frequency

- **Input:** Client sends at 30-60 Hz (or on input change)
- **State:** Server broadcasts at 60 Hz to all clients

## Usage

These messages form the core gameplay loop. The client sends input continuously, and the server broadcasts the authoritative state to all clients.

See individual message documentation for detailed specifications.
