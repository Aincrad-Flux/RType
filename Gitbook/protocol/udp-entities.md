---
icon: boxes
---

# UDP Messages - Entity Management

Messages for delta-based entity updates (reserved for future implementation).

## Message Types

| Type | Name | Direction | Status | Purpose |
|------|------|-----------|--------|---------|
| 5 | [Spawn](udp-05-spawn.md) | Server → Client | Reserved | Entity creation event |
| 6 | [Despawn](udp-06-despawn.md) | Server → Client | Reserved | Entity removal event |

## Current Status

These message types are **reserved** but not currently implemented. The server uses the `State` message to send all entities every frame instead of delta updates.

## Future Implementation

When implemented, these messages will provide:
- **Reduced bandwidth:** Only send changes instead of full state
- **Event-driven updates:** Clear spawn/despawn events for clients
- **Better interpolation:** Clients can animate entity appearance/disappearance

## Usage

Currently unused. See individual message documentation for planned specifications.
