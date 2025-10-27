---
icon: bug
---

# Troubleshooting

## Conan/CMake errors

Symptom: `conan_toolchain.cmake not found`.
- Cause: the Conan step was not executed for this build type (Release/Debug).
- Fix: run `conan install . -of=build -s build_type=Release --build=missing` (or Debug),
  then `cmake --preset conan-release` and `cmake --build --preset conan-release`.

Symptom: system package installation fails.
- Cause: permissions required by the system package manager.
- Fix: re-run `conan install` with the required privileges.

## C++ build errors

Symptom: C++20 errors or missing headers.
- Check compiler version (`g++ --version`/`clang --version`).
- Clean and rebuild: remove `build/` then redo `conan install` + `cmake --preset` + `cmake --build`.

## Raylib/display

Symptom: window does not appear or crashes.
- Linux: ensure a graphical session (X11/Wayland). Install X11 dev if necessary (Conan may attempt to install it).
- SSH: use X11 forwarding or run locally.

## UDP networking

Symptom: no HelloAck / no entities.
- Verify the server is running and listening on the right port.
- Temporarily disable the local firewall or open the UDP port 4242.
- Verify the address entered in the client (127.0.0.1 for local).

Symptom: stutters or lag.
- UDP can lose or delay packets; interpolation on the client and smaller payloads can help.

## Protocol version

Symptom: packets ignored.
- Client and server must use `ProtocolVersion = 1`. If you modify `Protocol.hpp`, rebuild both.

## Useful logs

- Server: shows port/IP at startup; add logs in `UdpServer::handlePacket` for diagnosis.
- Client: logs connection steps and receive errors.
