# Server — Responsibilities and Lifecycle

This document describes precisely what the server does, what it manages, and how it cooperates with the engine and clients. No code is shown; the behavior is explained from the perspective of data, timing, and message flows.

## Responsibilities (authoritative server)

- Maintain an authoritative simulation of the game world.
- Accept players via UDP handshake and track their sessions.
- Integrate player inputs into the ECS simulation at a fixed tick rate.
- Spawn, update, and despawn entities such as players, enemies, bullets, and formations.
- Detect collisions and apply gameplay rules (damage, invincibility, scoring, lives).
- Periodically broadcast compact world snapshots to all connected clients.
- Publish asynchronous events (roster changes, lives updates, score updates, despawns, and control notifications).
- Handle timeouts and explicit disconnects, cleaning up server-side state.

## Threads and timing

- Networking thread: receives UDP datagrams and enqueues state changes (inputs, joins, leaves). It sends responses asynchronously without blocking the simulation.
- Game thread: runs the ECS simulation at a steady tick rate. The simulation step applies systems in sequence, updates entity states, and produces the world that will be serialized for clients.
- Broadcast cadence: world state is throttled to a fixed frequency to reduce bandwidth and avoid UDP fragmentation. If the world contains too many entities, only a prioritized subset is sent per update.

## Player lifecycle

1. Handshake: a player sends a versioned "hello" message, optionally including a username. If accepted, the server associates the sender endpoint with a new player entity and acknowledges the handshake.
2. Initialization: the player entity is created with transform, velocity, network type (player), color, input and shooting capabilities, charge-shot capability, size, and score. Initial lives are set to a fixed number.
3. Roster update: when a player joins, all clients receive a compact roster listing players, their identifiers, lives, and names (truncated). When a player leaves, the roster is sent again.
4. Input processing: inputs received from a client are stored and made available to the ECS systems on the next simulation step.
5. Timeout and disconnect: if no datagrams are received from a client for a period, the server removes that client. Clients can also send an explicit disconnect notice. Removal triggers a despawn event for that player's entity and a roster update to others.
6. Return-to-menu control: if the number of connected players falls below the required threshold, remaining clients receive a control notification to exit gameplay back to their menu.

## World simulation

- The engine systems handle movement integration, enemy formations and motion, bullet spawning (players and enemies), collision detection, temporary invincibility, and enemy wave spawning.
- After each tick, the server applies additional rules based on markers set by the systems (for example, when a player is hit). Lives are decremented and the player is respawned to a safe location with a brief invincibility period.
- The team score is computed as the sum of player scores. It is authoritative and broadcast whenever it changes.

## Serialization and broadcasting

- State snapshots: contain a header followed by a fixed-size list of packed entities, each carrying identifier, type, position, velocity, and color.
- Prioritization: players are included first, then bullets for responsiveness, then enemies. If the snapshot size would exceed a conservative UDP budget, lower-priority entities are truncated for that packet.
- Roster: includes a small header and a compact fixed-size record per player (identifier, lives, truncated name). This is sent on joins and leaves so UIs can update their top bar immediately.
- Lives updates: a small one-off message for a single player's new lives value, sent when a player is hit and loses a life.
- Score updates: a small one-off message with the current team-wide score, sent whenever it changes.
- Despawn: an event to notify clients that an entity (typically a player) should be removed locally.
- Control messages: for example, a request for clients to return to the menu when too few players remain.

## Input handling and validation

- Inputs are treated as desired movement and firing intents represented by a compact bitmask. The server applies these intents during the next simulation step.
- Charge-shot input is also represented as a bit and is processed by a dedicated system that determines when a beam should be emitted and at which thickness based on accumulated charge time.
- The server does not trust client-side physics; only inputs are accepted, while positions, bullets, collisions, lives, and score are authoritatively decided server-side.

## Scoring and lives

- Lives: each player starts with a fixed number of lives. On hit by an enemy projectile, one life is removed (down to a minimum of zero). The player is respawned with a brief invincibility window to avoid immediate repeated hits.
- Score: points are awarded to the player who fired the projectile that destroyed an enemy. Some enemies grant higher points (for example, shooter-type enemies). The team score (sum across all players) is broadcast to clients for display.

## Failure modes and edge cases

- Packet loss: snapshots are independent; missing one does not block future updates. Inputs are frequent and can be dropped without desynchronizing long-term state.
- Reordering: each datagram is self-contained or idempotent enough that out-of-order delivery is tolerated.
- Oversized worlds: snapshots truncate to a size budget; gameplay remains correct even if some cosmetic entities are omitted temporarily from a packet.
- Idle clients: timeouts remove inactive endpoints cleanly and inform remaining clients.
- Versioning: messages carry a protocol version; mismatched versions are ignored to prevent undefined behavior.

## Extension points

- New message types: add discrete control or event messages as needed (for example, round transitions, power-ups). Keep them small and self-describing via headers.
- New systems: introduce additional ECS systems for new gameplay features rather than growing existing ones.
- New enemy types or formations: create new component tags or formation patterns while preserving the snapshot packing rules.
- Matchmaking or lobbies: insert a layer in front of the current handshake and roster to group players before starting a session.
