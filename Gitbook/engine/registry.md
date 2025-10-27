---
icon: database
---

# Registry

The ECS registry is responsible for:

- Allocating entity identifiers and destroying entities
- Storing component pools by type
- Iterating systems in a deterministic order

Implementation notes:
- Favor contiguous storage for iteration hotspots
- Keep component APIs minimal (create/get/remove)
- Avoid dynamic allocation during the hot path when possible

See the `engine/src` directory for implementation details.
