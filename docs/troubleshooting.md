# Troubleshooting

## Conan/CMake errors

Symptom: `conan_toolchain.cmake not found`.
- Cause: the Conan step was not executed for this build type (Release/Debug).
- Fix: `conan install . -of=build -s build_type=Release --build=missing` (or Debug),
  then `cmake --preset conan-release` and `cmake --build --preset conan-release`.

Symptom: System package installation failure.
- Cause: permissions required by the system package manager.
- Fix: re-run `conan install` with the required privileges (possibly via `sudo`).

## Build C++ errors

Symptom: C++20 errors or missing headers.
- Check the compiler version (`g++ --version`/`clang --version`).
- Clean and rebuild: delete `build/` then redo `conan install` + `cmake --preset` + `cmake --build`.

## Raylib/display

Symptom: window doesn't appear or crashes.
- Linux: ensure a graphical session (X11/Wayland). Install X11 dev if necessary (Conan may try to install it).
- SSH: use X11 forwarding or run locally.

## UDP networking

Symptom: no HelloAck / no entities.
- Verify that the server is running and listening on the correct port.
- Temporarily disable the local firewall or open UDP port 4242.
- Verify the address entered in the client (127.0.0.1 for local).

Symptom: stutters/delays.
- Nature of UDP; latency and losses can cause jumps. Implement client interpolation or reduce payload to improve.

## Protocol version

Symptom: packets ignored.
- Server and client must use `ProtocolVersion = 1`. If you modify `Protocol.hpp`, rebuild both.

## Useful logs

- Server: displays port/IP at startup; add logs in `UdpServer::handlePacket` for diagnosis.
- Client: `Screens::logMessage` prints connection steps and receive errors.