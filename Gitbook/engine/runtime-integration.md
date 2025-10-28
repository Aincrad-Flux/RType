---
icon: layers
---

# Runtime integration

How the engine connects to the server loop:

- Simulation runs at a fixed tick (e.g., 60 Hz)
- Systems are executed in order each tick
- After the tick, a snapshot is serialized for clients
- Input intents received since the previous tick are applied during the next tick

Tips:
- Keep per-tick work predictable; avoid blocking I/O
- Separate networking from simulation threads; use lock-free queues or minimal critical sections
