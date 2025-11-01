---
icon: plug
---

# UDP Messages - Connection

UDP connection messages establish the gameplay session between client and server.

## Message Types

| Type | Name | Direction | Purpose |
|------|------|-----------|---------|
| 1 | [Hello](udp-01-hello.md) | Client → Server | UDP handshake with token and player name |
| 2 | [HelloAck](udp-02-hello-ack.md) | Server → Client | Acknowledges successful UDP session |

## Connection Flow

```
Client                    Server
  |                          |
  | [Received token from TCP]|
  |                          |
  |-- UDP Hello (token, name)->|
  |                          |
  |              [Validate]   |
  |              [Create player]|
  |                          |
  |<-- UDP HelloAck ----------| ✓ Session established
  |                          |
  | [Ready for gameplay]      |
  |                          |
```

## Usage

These messages are sent once at the start of the UDP session, after the TCP handshake completes.

See individual message documentation for detailed specifications.
