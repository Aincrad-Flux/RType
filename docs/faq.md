# FAQ

## How do I build the project?
See `docs/setup.md` and follow the Conan + CMake flow (`conan install`, then `cmake --preset` and `cmake --build`). Conan will download raylib and asio automatically.

## How do I run the server and client?
`make run-server` (port 4242) then `make run-client`. In the client, enter 127.0.0.1:4242 in Multiplayer.

## The client doesn't receive state, what should I do?
- Verify that the protocol version is the same (see `Protocol.hpp`, version=1)
- Disable the local firewall or allow UDP on port 4242
- Ensure that the client has sent Hello and received HelloAck (see client logs)

## Why UDP?
Low latency and tolerance to light losses. Reliability can be added on top if necessary.

## Shooting (Space) does nothing?
The `InputShoot` bit is sent, but the server doesn't have projectile logic yet. Coming soon.

## I see enemies "jumping" or disappearing
The server removes off-screen enemies and broadcasts state snapshots. Small stutters are normal without interpolation.

## Where is the detailed protocol?
`docs/protocol.md` (messages, sizes, endianness, etc.).

## Where is the ECS and how do I use it?
`src/engine/rt/ecs/*`. The `Registry` allows creating entities and managing components. Systems execute logic in `update(dt)`.

## How do I change the server port?
Run `r-type_server <port>` or modify the `run-server` command in the Makefile. Update the port on the client side.

## Are Windows/macOS supported?
The project is portable with Conan + CMake. Raylib and asio are cross-platform. Tested mainly on Linux/macOS.