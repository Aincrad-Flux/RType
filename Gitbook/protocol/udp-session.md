---
icon: log-out
---

# UDP Messages - Session Control

Messages for controlling game session state and flow.

## Message Types

| Type | Name | Direction | Purpose |
|------|------|-----------|---------|
| 13 | [ReturnToMenu](udp-13-return-to-menu.md) | Server → Client | Server requests client return to menu |

## Usage

The `ReturnToMenu` message is sent by the server when the game session should end, such as:
- Too few players remain to continue
- Game over condition reached
- Server shutting down gracefully
- Round/match completed

## Message Flow

```
Client                    Server
  |                          |
  | [Game in progress]       |
  |                          |
  | [Too few players]       |
  |                          |
  |<-- ReturnToMenu ---------|
  |                          |
  | [Show menu screen]      |
  | [Disconnect UDP]        |
  |                          |
```

## Client Behavior

On receiving `ReturnToMenu`:
1. Display appropriate message to user
2. Return to main menu screen
3. Optionally close UDP connection
4. Allow user to reconnect or exit

See individual message documentation for detailed specifications.
