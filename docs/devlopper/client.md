# Client — UI, Networking, and Rendering

This document explains how the client application is structured, how it transitions between screens, how it talks to the server, and how it renders the game. No source code is shown; this is a functional description for developers.

## UI flow and state machine

The client runs a simple state machine:
- Menu: landing screen with navigation to multiplayer.
- Multiplayer: connection form with username, server address, and port. On connect, the client attempts a handshake and transitions to Waiting on success.
- Waiting: connected to the server, polling for snapshots. When enough players are detected in the world snapshot, the client transitions to Gameplay. A cancel button returns to Menu.
- Gameplay: renders the latest world snapshot, sends inputs periodically, and draws the HUD with lives, team score, and shot mode. It also responds to server control messages (for example, return to menu) and shows a game-over overlay when the local player's lives reach zero.
- NotEnoughPlayers: informational screen shown when the server requests a return to menu because too few players remain.
- Exiting: application shutdown path.

Pressing the escape key from sub-screens returns to Menu and initiates a clean session leave (including an explicit disconnect notice). On final exit, graphics are unloaded and any active session is left.

## Asset handling

- The client expects sprite assets to be present in a known relative folder. If missing, it still runs the UI and networking but avoids drawing fallback placeholder shapes to keep presentation clean.
- Player sprites come from one sheet with a fixed grid; each remote player is assigned a persistent row to provide identity.
- Enemy sprites are taken from another sheet with a known grid. A specific frame is sampled for simplicity.

## Networking behavior

- A persistent UDP socket is opened and set non-blocking. A resolver determines the server endpoint from address and port.
- Handshake: upon connecting and upon entering gameplay, the client sends a versioned hello message carrying the username. It waits briefly for a handshake acknowledgement while also accepting other early server messages such as roster or score update.
- Input: the client builds a compact bitmask from pressed keys and sends inputs at a capped frequency. It supports two firing modes: normal (continuous small bullets) and charge (hold to charge, release to fire a beam). The mode toggles on a control key.
- Receive: each frame, the client drains a small batch of UDP datagrams to keep latency low without starving rendering. Messages are dispatched by type to update world snapshot, roster/identity, lives, score, and control state.
- Disconnect: when leaving the session or exiting, the client sends an explicit disconnect notice and closes the socket.

## Input processing and gating

- Inputs are gated against the latest known self position to prevent the local player from moving outside the playable area as rendered. This is a presentation safeguard; the server remains authoritative on true positions.
- The charge-shot is exposed as a separate bit that is set while the shot is held in charge mode. Normal mode sets the shoot bit only. The local beam visual is timed for feedback while the server performs actual hit resolution.

## Rendering and HUD

- Entities are rendered from the latest snapshot. Players are drawn using assigned sprite rows and a consistent scale; enemies use a sampled frame cropped for better fit; bullets are drawn as small rectangles.
- The top HUD shows other players’ names and up to a small number of lives icons per player. The local player’s lives are shown on a bottom bar alongside the team score and the current shot mode.
- The playable area is the region between the top bar and a bottom status bar. Rendering and input gating respect these bounds to avoid overlap with UI elements.

## Game over and control messages

- When the local player’s lives reach zero, a semi-transparent overlay announces game over with the current score. Pressing escape returns to the menu and tears down the session.
- When a control message requests returning to the menu (such as when too few players remain), the client immediately leaves the session and transitions to the informational screen, then back to the menu on user input.

## Error handling and robustness

- Missing assets: the client warns and suppresses sprite rendering, keeping UI responsive.
- Network issues: the client tolerates packet loss and reordering by treating snapshots as replaceable and updates as latest-wins.
- Non-blocking I/O and small receive batches ensure the UI remains smooth even on busy networks.

## Extensibility

- UI: new screens or options (for example, settings or leaderboards) can be added to the state machine and drawn in the same style.
- HUD: additional stats (per-player score, power-ups) can be introduced with minimal changes, sourced from new protocol messages.
- Art: swap or extend spritesheets as long as frame sizes and sampling logic are updated accordingly.
- Inputs: new actions can be represented by additional bits in the input mask and handled by corresponding systems on the server.
