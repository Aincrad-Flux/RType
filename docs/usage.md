# Usage

This guide explains how to run the server and client, and what to expect.

## Quick start

1) Build:
```bash
make build
```

2) Start the server (default UDP port 4242):
```bash
make run-server
```
The server prints the detected IP and the port. Keep it running.

3) Start the client:
```bash
make run-client
```

4) In the client, choose Multiplayer and enter:
- Username: any non-empty string
- Server address: 127.0.0.1 (or the server LAN IP)
- Port: 4242

5) Connect. On success, the Gameplay screen appears and entities start updating.

## Controls

- Arrow keys: move (Up/Down/Left/Right)
- Space: shoot bit is sent to server (not yet handled server-side)
- Esc: go back to menu or exit

## What you’ll see

- A starfield background
- Your player as a small rectangle; enemies as circles slowly moving left
- A HUD at top-left: username, Lv, and a placeholder HP bar

## Networking flow (high level)

- Client sends a Hello to server; server responds HelloAck and spawns a player entity
- Client sends Input packets at ~30 Hz with current input bits
- Server broadcasts State packets at ~60 Hz with all entities
- Client renders the latest received state

For binary message details, see `docs/protocol.md`.

## Common commands

```bash
make setup        # install deps via conan
make build        # build all targets
make build-server # build only server
make build-client # build only client
make run-server   # run server (4242)
make run-client   # run client
make clean        # remove build/
make re           # clean and rebuild
```

## Notes

- Firewalls or NAT may block UDP; for local tests use 127.0.0.1.
- If the Window closes immediately or can’t open, ensure raylib can access a display (X11/Wayland on Linux).