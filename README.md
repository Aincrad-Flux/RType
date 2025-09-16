# R-Type (C++/CMake, ECS, UDP, raylib)

This is a scaffold for the R-Type project using:
- CMake + Conan for dependency management
- Engine with an ECS core (`rt::ecs`)
- UDP server using standalone Asio
- Client using raylib for rendering

## Build (macOS/Linux)

Prereqs: `conan >=2`, CMake, a C++20 compiler. On macOS, Xcode CLI tools.

Commands:

```
make build        # installs deps, configures, builds
make run-server   # runs server on UDP 4242
make run-client   # runs raylib client
make clean        # remove build/
```

Artifacts are in `build/bin`: `r-type_server`, `r-type_client`.

## Layout

- `src/common`: shared protocol types
- `src/engine`: minimal ECS core (Registry, System, Storage)
- `src/server`: UDP server stub using Asio
- `src/client`: raylib window with placeholder starfield

## Next Steps

- Flesh out binary protocol, message serialization and validation
- Add systems for movement, rendering adapters, and networking
- Implement server tick loop with fixed timestep and entity replication
- Replace placeholder client rendering with real sprites and inputs