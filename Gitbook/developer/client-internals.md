---
icon: monitor
---

# Client internals

- State machine: Menu → Multiplayer → Waiting → Gameplay → (ReturnToMenu / GameOver) → Menu
- Assets: sprite sheets for players and enemies; gameplay continues with UI even if assets are missing
- Networking: non-blocking UDP socket; Hello/HelloAck; Input at a capped rate; batch receive per frame; explicit disconnect on leave
- Input: arrows for movement; Space for shoot; optional charge-shot mode with a separate bit
- Rendering: latest snapshot; HUD with players’ lives and team score; bounded playable area between top and bottom bars
- Robustness: tolerates packet loss/reordering; suppresses heavy visuals when assets are missing
