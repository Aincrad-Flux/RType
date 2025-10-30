---
icon: question
---

# FAQ

## How do I build the project?
See Setup and follow the Conan + CMake flow (`conan install`, then `cmake --preset` and `cmake --build`). Conan will download raylib and asio automatically.

## How do I run the server and client?
Build, then run the server (port 4242) and the client. In the client, select Multiplayer and use 127.0.0.1:4242 for local tests.

## The client receives no state — what should I check?
- Ensure protocol version matches (see `include/common/Protocol.hpp`, `ProtocolVersion=1`).
- Disable local firewall or allow UDP on port 4242.
- Check that the client sent Hello and received HelloAck (see client logs).

## Why UDP?
Low latency and tolerance to small losses. Reliability can be layered above if needed.

## Pressing Space doesn't shoot?
The `InputShoot` bit is sent, but the server’s projectile logic may not be implemented yet.

## I see enemies “jump” or disappear
The server prunes off-screen enemies and sends periodic snapshots of state. Small stutters are normal without client interpolation.

## Where is the detailed protocol?
See Protocol.

## Where is the ECS and how do I use it?
See the Engine section; the `Registry` creates entities and manages components. Systems run logic in `update(dt)`.

## How do I change the server port?
Run `r-type_server <port>` or adjust your run command. Update the client’s port accordingly.

## Is Windows/macOS supported?
The project is portable with Conan + CMake. Raylib and asio are cross-platform. Most testing is done on Linux/macOS.
