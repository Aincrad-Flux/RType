---
icon: server
---

# TCP Messages

TCP messages are used for initial connection setup and reliable delivery. They are sent over a TCP connection established before UDP gameplay begins.

## Message Types

| Type | Name | Direction | Purpose |
|------|------|-----------|---------|
| 100 | [TcpWelcome](tcp-100-welcome.md) | Server → Client | Provides UDP port and authentication token |
| 101 | [StartGame](tcp-101-start-game.md) | Server → Client | Signals game is ready to start |

## Connection Flow

```
Client                    Server
  |                          |
  |-- TCP Connect ---------->|
  |                          |
  |<-- TcpWelcome -----------| (UDP port + token)
  |                          |
  |<-- StartGame ------------| (Game ready)
  |                          |
  |-- Close TCP (optional) -->|
  |                          |
  |-- Switch to UDP --------->|
  |                          |
```

## Usage

TCP messages are only exchanged during the initial connection phase. Once gameplay begins, all traffic switches to UDP for lower latency.

See individual message documentation for detailed specifications.
