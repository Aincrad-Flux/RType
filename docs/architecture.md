# Architecture

This document describes the main components of the project and how they interact.

## Overview

R-Type is split into four modules:

- Common: shared protocol definitions used by client and server
- Engine: a minimal ECS kernel
- Server: an authoritative UDP server that simulates entities and broadcasts world state
- Client: a raylib-based UI/game client that connects via UDP and renders entities

## Module details

### Common (`src/common`)

Defines the binary network protocol in `include/common/Protocol.hpp`:
- `Header` (size/type/version)
- `MsgType` enum (Hello, HelloAck, Input, State, ...)
- Input bitmask, `InputPacket`
- `PackedEntity`, `StateHeader`

See `docs/protocol.md` for precise wire format and sizes.

### Engine (`src/engine`)

Small ECS kernel with:
- `rt::ecs::Registry`: creates/destroys entities, stores components, runs systems
- `ComponentStorage<T>`: per-type sparse storage
- `System`: interface for update(dt)

The client/server currently don’t use many components yet; the kernel is ready for future systems.

### Server (`src/server`)

Key files:
- `UdpServer.hpp/.cpp`: UDP socket, async receive, tick thread, broadcast state
- `main.cpp`: boot banner and server lifecycle

Responsibilities:
- Accept Hello, create a Player entity (assign ID, initial pos, color)
- Update Player positions from last received input bits
- Spawn enemies periodically and prune them when off-screen
- Broadcast State packets (~60 Hz) containing all entities

Data ownership:
- The server is authoritative on entity list and transforms
- Players are identified by their endpoint (addr:port) and mapped to a `playerId`

### Client (`src/client`)

Key files:
- `ui/App.[hc]pp`: window loop and screen routing
- `ui/Screens.[hc]pp`: all UI and networking logic for gameplay
- `ui/Widgets.[hc]pp`: immediate-mode UI helpers

Responsibilities:
- Menus (Singleplayer/Multiplayer/Options/Leaderboard)
- UDP networking: Hello handshake, send Input, receive State (non-blocking)
- Rendering with raylib: starfield, HUD, and entity sprites (placeholder shapes)

### Build system

- Conan 2 fetches `raylib` and `asio`
- CMake generates targets: `rtype_common`, `rtype_engine`, `r-type_server`, `r-type_client`
- Makefile wraps common flows: setup/build/run

## Data flow

1) Client sends Hello; server responds HelloAck and registers the endpoint -> player entity created
2) Client sends Input packets periodically
3) Server runs a 60 Hz tick, updates entity positions
4) Server broadcasts State packet to all clients
5) Client renders the latest received state

## Design notes

- UDP chosen for low latency; reliability left to higher layers if needed
- Binary format is pack(1) for large payloads (`State`); header remains native layout
- Current implementation prioritizes simplicity; security and validation are minimal and documented in `docs/protocol.md`