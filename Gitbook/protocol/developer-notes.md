---
icon: file-code
---

# Protocol — developer notes

This page explains message semantics and design choices for implementers.

- Header: payload-size, type, version. Mismatched versions are ignored.
- Hello/HelloAck: minimal handshake so the server can register the endpoint and spawn a player entity.
- Input: compact bitmask for up/down/left/right/shoot/charge with an optional sequence id.
- State: periodic snapshot of (id, type, position, velocity, color) for entities. Prioritized to fit within a safe UDP budget.
- Roster/Lives/Score/Control: one-shot updates allowing responsive UI without waiting for a snapshot.
- Reliability: UDP best effort. Snapshots replace state; updates are latest-wins.
- Extensions: add new fixed-size messages for features (round states, power-ups). Keep payloads small and self-contained.

See the wire-format specification for field sizes and alignments.
