# R-Type Client UI Refactor

This client has been split into small, single-responsibility files. Each file does one thing (one widget, one screen, or one support concern) to make the code easier to navigate and edit.

## Layout

- include/client/ui/
  - App.hpp               — App lifecycle and screen state enum/types
  - Screens.hpp           — Screens class declaration and shared types
  - Widgets.hpp           — Umbrella header re-exporting individual widgets
  - widgets/
    - Button.hpp          — Button widget API
    - InputBox.hpp        — Input box widget API
    - Title.hpp           — Centered title text API
- src/ui/
  - App.cpp               — Window + main loop + screen routing
  - widgets/
    - Button.cpp          — Button implementation
    - InputBox.cpp        — Input box implementation
    - Title.cpp           — Title implementation
  - screens/
    - Menu.cpp            — Main menu screen
    - Singleplayer.cpp    — Singleplayer form
    - Multiplayer.cpp     — Multiplayer form + connect flow
    - Waiting.cpp         — Waiting-for-players lobby
    - Gameplay.cpp        — In-game rendering + HUD + inputs
    - Options.cpp         — Placeholder
    - Leaderboard.cpp     — Placeholder
    - NotEnoughPlayers.cpp— Return-to-menu screen when a player leaves
    - Assets.cpp          — Sprite asset discovery and loading/unloading
    - Net.cpp             — UDP socket lifecycle and send helpers
    - NetPackets.cpp      — Packet parsing and local state updates
    - Utils.cpp           — Simple logging helper

## If you want to work on ONLY 3 files
Pick a profile depending on your focus:

- UI flow focused:
  1. src/ui/App.cpp           — event loop and screen switching
  2. src/ui/screens/Menu.cpp  — menu and navigation
  3. src/ui/screens/Gameplay.cpp — gameplay rendering/HUD (most visible)

- Gameplay/network focused:
  1. src/ui/screens/Gameplay.cpp — input, HUD, entity drawing
  2. src/ui/screens/Net.cpp      — network send/connect/disconnect
  3. src/ui/screens/NetPackets.cpp — parsing server packets

Widgets are already small and can be edited in isolation if needed.

## Build
CMake is updated to include the new files. You can build the workspace task "build-rtype".
