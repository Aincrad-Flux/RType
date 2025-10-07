# Setup

This guide explains how to prepare your environment and build the project.

## Prerequisites

- A C++20 compiler (GCC 11+/Clang 13+)
- CMake 3.27+ (installed automatically via Conan tool_requires if needed)
- Conan 2.x (Python package manager for C/C++)
- Git
- Linux/macOS/Windows (tested Linux/macOS)

Verify tools:

```bash
conan --version
cmake --version
g++ --version   # or clang --version
```

Install Conan (if missing):

```bash
pip install --user conan
conan profile detect
```

## Dependencies

Conan will fetch third-party libs:
- raylib/5.5 (client rendering)
- asio/1.28.0 (networking)

No manual install is required in most cases. On Linux, some system packages may be auto-installed; if permissions are needed, re-run the setup with sudo (see below).

## First-time setup

```bash
make setup           # installs Conan deps and generates toolchain files
# If it fails due to permissions on system packages:
sudo make setup-sudo
```

Notes:
- The Makefile calls `conan install` into `build/`, producing `conan_toolchain.cmake` and CMakeDeps.
- Preset is controlled via PRESET=Release|Debug (default Release).

## Configure and build

Build everything:

```bash
make build           # runs setup + configure + build
```

Or build specific targets:

```bash
make build-server    # only r-type_server
make build-client    # only r-type_client
```

Artifacts are placed in `build/bin/` and also copied to project root as convenience binaries: `./r-type_server`, `./r-type_client`.

## Run

Server (UDP, port 4242 by default):

```bash
make run-server      # equivalent to build/bin/r-type_server 4242
```

Client (raylib window):

```bash
make run-client
```

Then, in the client Multiplayer screen, enter the server IP and port (e.g., 127.0.0.1:4242) and connect.

## Troubleshooting setup

- If `conan_toolchain.cmake` is not found when configuring: run `make setup` first.
- On corporate or minimal OS images, you might need build tools (e.g., `build-essential`, `pkg-config`, X11 dev for raylib on Linux). The Conan recipe will try to install system requirements; use `sudo make setup-sudo` if prompted.
- See `docs/troubleshooting.md` for common runtime issues (firewall/UDP, display, etc.).

## What’s next

- Read `docs/usage.md` to run and test the client/server.
- Read `docs/protocol.md` for the network protocol details.