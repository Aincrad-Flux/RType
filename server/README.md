# Server module layout

This server was refactored to separate responsibilities into clear modules:

- include/UdpServer.hpp, src/UdpServer.cpp
  Small facade that wires networking and gameplay together. It owns the two modules below and starts/stops them.

- include/network/NetworkManager.hpp, src/network/NetworkManager.cpp
  Owns the UDP socket (asio), async receive loop, endpoint bookkeeping, and provides send/broadcast helpers and timeout checks. It forwards incoming packets to a packet handler callback.

- include/instance/MatchInstance.hpp, src/instance/MatchInstance.cpp
  Session/lobby/match orchestration. Handles protocol packet semantics (Hello, Input, LobbyConfig, StartMatch, Disconnect), player roster/host, lives and scores, and the gameplay loop. Uses NetworkManager for all network I/O.

- include/gameplay/GameSession.hpp, src/gameplay/GameSession.cpp
  Thin wrapper around the ECS registry. Initializes all gameplay systems and exposes an `update(dt)` method.

## Notes
- Behavior and protocol are unchanged; code is just split for maintainability.
- If you need to customize networking behavior (ports, timeouts), look at `NetworkManager`.
- If you need to change gameplay or lobby rules, modify `MatchInstance` and/or `GameSession`.
