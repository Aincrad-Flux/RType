# R-Type

Modern C++ (C++20) implementation of the classic R-Type as a networked game with:
- A custom game engine based on an ECS architecture (see `engine/`)
- A multithreaded authoritative server using standalone Asio over UDP (+ TCP side-channel)
- A graphical client using raylib (OpenGL/X11 on Linux)

The project uses CMake and Conan 2 to be reproducible and self-contained.

## Features (from the subject)
- Networked gameplay: multiple clients connect to a central server over UDP.
- Authoritative server: spawns/moves/destroys entities and broadcasts events.
- Client renders the game, handles inputs, and stays in sync with server authority.
- ECS-driven engine to keep rendering, networking, and game logic decoupled.

See detailed docs in `docs/` and `Gitbook/`.

## Requirements
- Linux (client and server). Cross-platform is a stretch goal.
- CMake ≥ 3.20
- Conan 2.x
- A C++20 compiler (GCC, Clang, or MSVC on Windows)

Optional system libs (usually present on Linux): OpenGL and X11 runtime libraries. Conan will generate CMake targets for `opengl::opengl` and `xorg::xorg` if needed.

## Quick start (CMake + Conan)

First-time configure + build:

```bash
# 1) Configure (Conan will be invoked automatically by the root CMake)
cmake -B build -S .

# 2) Build (both client and server by default)
cmake --build build
```

Executables are written to the project root:
- `./r-type_server`
- `./r-type_client`

To build only one target:

```bash
cmake --build build --target r-type_server
cmake --build build --target r-type_client
```

To configure with a specific build type (e.g., Debug):

```bash
cmake -B build -S . -DCMAKE_BUILD_TYPE=Debug
cmake --build build
```

You can also use presets once the repo has been configured at least once:

```bash
# Configure + Build in one go
cmake --workflow --preset all
```

Notes:
- Avoid running CMake/Conan with `sudo`. If you previously did, fix permissions or clean the `build/` directory.
- The root CMake handles Conan (profile detection + install) and locates the generated toolchain automatically.

## Run

Server (UDP port defaults to 4242; TCP side-channel uses port+1):

```bash
./r-type_server            # UDP: 4242, TCP: 4243
./r-type_server 5000       # UDP: 5000, TCP: 5001
```

Client:

```bash
./r-type_client
```

## Project layout

- `common/` – shared code (protocol and helpers)
- `engine/` – ECS core and systems integration
- `server/` – authoritative server (Asio UDP/TCP)
- `client/` – raylib client and UI
- `docs/`, `Gitbook/` – architecture, protocol, and developer docs

## Troubleshooting
- Permission denied under `build/build/Release/generators`: you likely ran a previous configure with sudo.
	```bash
	sudo chown -R "$USER":"$USER" build  # or: sudo rm -rf build
	```
- Conan complains about an invalid build_type: pass one explicitly or let CMake default to Release.
	```bash
	cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
	```

## License
See `LICENSE.md`.