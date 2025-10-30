---
icon: blueprint
---

# Architecture

Modules:
- Common: shared protocol types
- Engine: minimal ECS kernel
- Server: authoritative UDP server (simulation and snapshots)
- Client: raylib-based game client (UI/networking/rendering)

Build:
- Conan 2 fetches `raylib` and `asio`
- CMake generates targets: `rtype_common`, `rtype_engine`, `r-type_server`, `r-type_client`
- Makefile wraps setup/build/run workflows

Data flow:
1. Client sends Hello; server responds HelloAck and registers player entity
2. Client sends Input periodically
3. Server ticks ECS at ~60 Hz, updates entities
4. Server broadcasts State snapshots
5. Client renders the latest snapshot
