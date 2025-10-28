---
icon: workflow
---

# CI/CD

Pipelines and helper scripts live under `jenkins/`:
- Clone, verify code, install dependencies
- Build specific binaries and all binaries
- Generate documentation and artifacts
- Deploy doxygen to a VM
- Send to official repository

Notes:
- Keep build steps reproducible with Conan + CMake
- Prefer release presets for publication artifacts
