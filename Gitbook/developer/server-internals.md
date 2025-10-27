---
icon: server
---

# Server internals

- Authoritative simulation over UDP
- Threads: networking (recv/send) and game (tick)
- Tick: run systems in order; apply post-tick rules (lives, respawn, team score)
- Broadcast: periodic State snapshots; one-shot Roster/Lives/Score/Control messages
- Player lifecycle: handshake → entity spawn → inputs → timeout/disconnect → despawn
- Prioritization: players first, then bullets, then enemies in snapshots
- Limits: cap entities per snapshot to avoid fragmentation; use conservative payload sizes
