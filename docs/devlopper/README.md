# R-Type — Developer Guide (Overview)

This guide explains the entire project in depth so a developer unfamiliar with the codebase can understand how it works and confidently modify it. It provides a narrative of the runtime architecture, the multiplayer server, the ECS-driven game engine, the binary network protocol, and the client application. No source code is shown; everything is described conceptually and operationally.

Sections:
- Project architecture and runtime data flow
- Server responsibilities and lifecycle
- Engine (ECS) model, components, and systems
- Network protocol semantics and message flows
- Client responsibilities, UI flow, input, and rendering
- How to extend or modify the project safely

Related files in this folder:
- server.md — server design and behavior
- engine.md — ECS architecture, components, and systems
- protocol.md — binary protocol and message flows
- client.md — client application design and UX

For general user-facing instructions, see the existing documentation in docs/ at the repository root.

## Project architecture and data flow

At a high level, the application is a networked shoot’em up with a thin client and an authoritative server. The server simulates the world at a fixed tick rate and periodically broadcasts compact world snapshots. Clients send minimal inputs and render whatever the server authoritatively reports.

Key roles at runtime:
- The Server runs a UDP listener, accepts player “hello” handshakes, maintains player sessions with timeouts and explicit disconnects, and simulates the world using an Entity–Component–System (ECS) engine. It periodically publishes authoritative state and asynchronous events to all connected clients.
- The Engine is embedded in the server and defines the data model (components) and behavior (systems) for players, enemies, bullets, waves, scoring, and survivability. It updates at a steady rate and applies gameplay rules deterministically.
- The Protocol is a small, versioned binary format tailored for low-latency updates over UDP. Clients send inputs; the server returns state, events, roster information, scoring updates, and control notifications.
- The Client presents the user interface, connects to a server over UDP, sends user inputs at a capped rate, renders the last known authoritative snapshot, and shows a HUD with lives and score. It also handles control messages such as “return to menu” when too few players remain.

Data flow summary:
- Client to Server: handshake, periodic inputs, explicit disconnect notice.
- Server to Client: handshake acknowledgement, periodic world state, roster updates on joins/leaves, lives and score updates, despawn notices, and control notifications.

## Simulation and timing

- The Engine updates at a steady rate on the server to advance motion, spawn entities, handle collisions, and apply rules (such as invincibility windows after hits).
- World snapshots are throttled to a fixed frequency to reduce bandwidth and avoid UDP fragmentation. Entity packing is ordered to prioritize critical objects (players first, then bullets, then enemies) under size constraints.
- Clients process a small batch of incoming datagrams each frame to keep the UI responsive and prevent backlog bursts. Inputs are sent at a capped frequency to balance responsiveness and bandwidth.

## Extending the project safely

When adding or modifying features, keep these guidelines in mind:
- Prefer adding new ECS components and systems rather than expanding existing ones in ad-hoc ways. This keeps behavior modular and testable.
- Introduce new protocol message types for features that require client–server coordination. Keep payloads compact and resistant to partial delivery or reordering.
- Maintain authoritative decisions server-side (for scoring, entity spawns, damage, lives). The client should remain presentation-focused.
- Respect size budgets for UDP packets and snapshot frequency. Favor multiple small messages over one oversized datagram.
- Preserve UI flows: users go from menu to multiplayer, then waiting until enough players arrive, then gameplay, and back to menu on exit or server request.
- Keep timeouts and explicit disconnect handling intact so the server cleans up sessions reliably.

Read the detailed sections for specific mechanics and extension points:
- See server.md for networking threads, player lifecycle, and broadcast logic.
- See engine.md for component and system responsibilities and world rules.
- See protocol.md for message semantics, sizes, ordering, and throttling.
- See client.md for the UI state machine, asset loading, input gating, and HUD logic.
