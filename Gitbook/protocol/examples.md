---
icon: terminal
---

# Examples

Code references and tiny probes.

- Protocol definitions:
  - `common/include/common/Protocol.hpp`
- Server networking:
  - `server/include/UdpServer.hpp`
  - `server/src/UdpServer.cpp`
- Client networking:
  - `client/src/net/Net.cpp`
  - `client/src/net/NetPackets.cpp`

Optional quick checks (for documentation only):
- Print header sizes in a small program to confirm ABI (expect Header=4, InputPacket=5, PackedEntity=25 with pack(1) where applicable)
- Send a synthetic Hello datagram from a UDP tool and observe HelloAck response
