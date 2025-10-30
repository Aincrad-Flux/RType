---
icon: boxes
---

# Build & dependencies

- Package manager: Conan 2
  - raylib/5.5 for rendering (client)
  - asio/1.28.0 for UDP networking
- Build system: CMake 3.27+
- Presets: `conan-release` and `conan-debug`

Quick flow:
```bash
conan install . -of=build -s build_type=Release --build=missing
cmake --preset conan-release
cmake --build --preset conan-release
```

Artifacts: `build/Release/bin/` (or `build/Debug/bin/`).
