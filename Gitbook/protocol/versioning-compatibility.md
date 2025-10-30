---
icon: git-branch
---

# Versioning & compatibility

- Current protocol version: `1`
- Clients and server must match the version; mismatched messages are ignored
- When changing the protocol:
  - Bump the version constant in `Protocol.hpp`
  - Keep old parsing paths if you need cross-version grace periods
  - Prefer additive, fixed-size changes over in-place field repurposing
