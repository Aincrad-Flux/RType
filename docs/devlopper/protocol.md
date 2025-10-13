# Protocol — Binary Message Semantics

This document explains how the client and server communicate using a compact binary protocol over UDP. The goal is low latency with small, self-contained messages. No code is shown; this is a semantic description for implementers.

## General framing and versioning

- Each datagram begins with a fixed-size header containing payload size (excluding header), message type, and protocol version.
- The protocol version is checked by both sides. Messages with mismatched versions are ignored.
- Most messages are one-shot notifications or small snapshots. There is no stream or connection state beyond what is explicitly tracked by endpoints on the server.

## Message types (semantic overview)

- Hello: client → server. Initiates a session. Optionally carries a short username. The server registers the sender endpoint and creates a player entity.
- HelloAck: server → client. Confirms the handshake so the client can move from connect to waiting.
- Input: client → server. Carries a small bitmask (up, down, left, right, shoot, charge) and a client-side sequence id. The server reads intents each tick.
- State: server → client. A periodic world snapshot containing a small header (entity count) followed by packed entity records with id, category, position, velocity, and color. Ordered by priority to fit in a conservative UDP size budget.
- Spawn: (reserved for future use). Would announce a new entity explicitly if needed outside of snapshots.
- Despawn: server → client. Informs clients to remove an entity, typically on player disconnect.
- Ping/Pong: (reserved). Not used in the current implementation; could support latency checks in the future.
- Roster: server → client. Announces the list of connected players (identifier, lives, truncated username). Sent on joins or leaves so UIs can update immediately.
- LivesUpdate: server → client. Announces a single player's lives change after a hit and respawn.
- ScoreUpdate: server → client. Announces the latest authoritative team score (sum across players). The identifier may be unused for team-wide totals.
- Disconnect: client → server. Explicit notice that a client is leaving voluntarily.
- ReturnToMenu: server → client. Control message asking clients to leave gameplay, typically because too few players remain.

## Input encoding

- Inputs are represented as a single byte with individual bits for up, down, left, right, shoot, and charge. The shoot bit indicates continuous fire; the charge bit indicates the charge button is currently held. Clients may choose to set only one of these dependently on a UI mode.
- A monotonically increasing sequence id is included by clients for potential future reconciliation; the current server design accepts inputs as latest-intent without re-simulation.

## World snapshots

- Each snapshot begins with a small header stating how many entities follow.
- Each entity is represented in a fixed-size record with fields for identifier, category, position, velocity, and color. Categories include player, enemy, and bullet. This fixed layout enables fast parsing without dynamic memory.
- To avoid IP fragmentation, snapshots are capped to a size safely below typical MTU limits. If the world contains more entities than can fit, the server prioritizes players first, then bullets, then enemies. Truncation only affects visual completeness for a single frame and recovers on the next snapshot.

## Roster and player identity

- The server assigns an identifier to each player entity when the handshake is processed.
- Usernames are truncated to a small, fixed length and stored as zero-terminated text in a compact per-player record.
- The roster message contains the full list of players at the time of sending. Clients use it to map their local identity (by matching their truncated username) and to display teammates and their lives.

## Lives and score updates

- When a player is hit by an enemy projectile and loses a life, the server sends a small update message containing the player's identifier and new lives value.
- The score update message carries the latest authoritative team score. The server sends it whenever the sum across all player scores changes (for example, when destroying enemies). Clients treat this as a total rather than per-player breakdown.

## Control messages and disconnects

- When only one player remains connected (below the minimum required to continue), the server asks all remaining clients to return to the menu via a control message. Clients must treat this as a state transition out of gameplay.
- Clients should send a disconnect notice when leaving voluntarily to allow immediate cleanup on the server, though timeouts will eventually remove stale endpoints.

## Reliability and ordering considerations

- UDP delivery is best effort; messages can be lost or reordered. The protocol is designed so that a lost snapshot is harmless (the next one replaces it), and one-off updates (roster, lives, score) can be processed out of order without corrupting state. The latest update replaces previous knowledge.
- Clients process a small batch of messages per frame to keep the UI responsive and avoid starving rendering.

## Extending the protocol

- New message types can be added for features such as waves, power-ups, damage types, or match states. Keep each message self-contained and small.
- When introducing new payloads, prefer fixed-size fields and keep text truncated to prevent dynamic parsing complexity.
- If larger state must be sent, consider splitting across several datagrams or increasing the snapshot frequency temporarily rather than risking fragmentation.
