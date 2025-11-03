---
icon: users
---

# UDP Messages - Player Management

Messages related to player roster, lives, scores, and disconnections.

## Message Types

| Type | Name | Direction | Purpose |
|------|------|-----------|---------|
| 9 | [Roster](udp-09-roster.md) | Server → Client | Player list with names and lives |
| 10 | [LivesUpdate](udp-10-lives-update.md) | Server → Client | Player lives changed |
| 11 | [ScoreUpdate](udp-11-score-update.md) | Server → Client | Player score changed |
| 12 | [Disconnect](udp-12-disconnect.md) | Client → Server | Client disconnect notification |

## Message Flow

```
Client                    Server
  |                          |
  | [Player joins]           |
  |<-- Roster --------------| (All players)
  |                          |
  | [Player loses life]     |
  |<-- LivesUpdate ----------| (Player X: 2 lives)
  |                          |
  | [Player scores]         |
  |<-- ScoreUpdate ----------| (Player X: 150 points)
  |                          |
  | [Player leaves]         |
  |-- Disconnect ----------->|
  |<-- Roster --------------| (Updated list)
  |                          |
```

## Frequency

- **Roster:** Sent on join/leave or when lives change significantly
- **LivesUpdate:** Sent immediately when any player loses a life
- **ScoreUpdate:** Sent immediately when any player's score changes
- **Disconnect:** Sent once when client gracefully disconnects

See individual message documentation for detailed specifications.
