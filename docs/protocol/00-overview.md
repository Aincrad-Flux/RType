# Protocol Overview

## Purpose

The R-Type network protocol is a lightweight binary protocol designed for real-time multiplayer gameplay. It prioritizes low latency and minimal bandwidth usage over reliability and security.

## Design Philosophy

### Key Principles

1. **Minimal Overhead:** Binary encoding with packed structures to reduce packet size
2. **Server Authority:** Server is authoritative for all game state
3. **Latest State Wins:** No retransmission; clients always apply the most recent state
4. **Input-State Loop:** Clients send inputs, server broadcasts authoritative state

### Architecture Pattern

```
Client                           Server
  |                                |
  |-- TCP Connect --------------->|
  |<-- TcpWelcome (UDP port) -----|
  |<-- StartGame (token) ---------|
  |                                |
  |-- UDP Hello (token) --------->|
  |<-- UDP HelloAck ---------------|
  |                                |
  |-- Input (60 Hz) ------------->|
  |<-- State (60 Hz) --------------|
  |<-- Roster (on changes) --------|
  |<-- LivesUpdate (on changes) ---|
  |<-- ScoreUpdate (on changes) ---|
  |                                |
```

## Protocol Characteristics

### Transport Layers

- **TCP:** Initial connection, room/lobby communication, reliable delivery needed
- **UDP:** All gameplay traffic, prioritizes speed over reliability

**Rationale:** TCP handles connection setup and authentication token exchange. Once gameplay begins, UDP provides the low latency required for real-time action.

### Binary Encoding

All messages use binary encoding with the following characteristics:

- **Endianness:** Little-endian (native machine format, no conversion)
- **Struct Packing:** `#pragma pack(push, 1)` for all payload structures
- **Alignment:** No padding bytes between fields
- **Floats:** IEEE-754 32-bit single precision

**Warning:** This protocol is **not portable** across different endianness architectures without modification. All clients and servers must run on little-endian systems (x86, x86-64, ARM in little-endian mode).

### Versioning

**Current Version:** 1

Version checking:
- Every message header contains a `version` field
- Server **silently ignores** messages with `version != 1`
- No backward compatibility mechanism currently exists

**Future Versions:** When breaking changes are needed, increment the protocol version and implement version-specific parsing.

## Message Flow Patterns

### 1. Initial Connection (TCP)

1. Client connects to server TCP port (default: 4242)
2. Server sends `TcpWelcome` with UDP port number
3. Server sends `StartGame` with authentication token
4. Client closes TCP connection (or may keep open for chat/lobby features)

### 2. UDP Session Establishment

1. Client sends UDP `Hello` to server UDP port with token from TCP
2. Server validates token, creates player entity, sends `HelloAck`
3. Server includes player in `State` broadcasts
4. Server sends initial `Roster` with all player info

### 3. Active Gameplay Loop

**Client → Server (Input Stream):**
- Client sends `Input` messages at 30-60 Hz (or on input change)
- Each input contains sequence number and button bitmask
- Server applies latest received input for each player

**Server → Client (State Stream):**
- Server broadcasts `State` at 60 Hz to all clients
- State contains all entities: players, enemies, bullets
- Clients render based on latest state (with optional interpolation)

**Server → Client (Events):**
- `Roster`: Sent when players join/leave
- `LivesUpdate`: Sent when any player loses a life
- `ScoreUpdate`: Sent when any player's score changes
- `ReturnToMenu`: Sent when game ends (e.g., too few players)

### 4. Graceful Disconnect

- Client sends `Disconnect` message to server
- Server removes player entity and sends updated `Roster` to remaining clients

### 5. Ungraceful Disconnect

- Server may use timeout mechanism (if `Ping/Pong` implemented)
- Currently: players who stop sending input remain in game until server restart

## Performance Characteristics

### Bandwidth Usage

**Per Client (Approximate):**

**Outbound (Client → Server):**
- `Input`: 9 bytes/packet × 60 Hz = 540 bytes/sec = **4.3 Kbps**

**Inbound (Server → Client):**
- `State` header: 4 + 2 = 6 bytes
- Per entity: 25 bytes
- Example with 10 entities: (6 + 250) × 60 Hz = 15,360 bytes/sec = **123 Kbps**
- Worst case (512 entities): (6 + 12,800) × 60 Hz = 768,360 bytes/sec = **6.1 Mbps**

**Server Total (N clients):**
- Receive: 4.3 Kbps × N
- Send: 123 Kbps × N² (each state goes to each client)
- Example: 10 clients = 43 Kbps in, 12.3 Mbps out

### Latency Considerations

**Minimum Round-Trip Time Components:**
1. Client input serialization: <1ms
2. Network transit: variable (LAN: 1-10ms, WAN: 20-100ms+)
3. Server processing: <1ms (at 60 Hz tick rate)
4. Network transit return: same as #2
5. Client state parsing and rendering: <16ms (at 60 FPS)

**Total RTT:** Network latency × 2 + ~17ms processing

**Input Lag:** In the worst case, an input sent just after a server tick will wait ~16ms for the next tick, plus network RTT, plus one frame of display lag.

## Reliability and Ordering

### UDP Unreliability

**Packet Loss:**
- Server sends state at 60 Hz; loss of individual states is acceptable
- Client should always apply the latest state, even if it missed intermediate states
- 5% packet loss = client sees 57 states/second instead of 60 (still playable)

**Out-of-Order Delivery:**
- Server should timestamp states or use sequence numbers (not currently implemented)
- Without sequence numbers, out-of-order states may cause brief visual glitches
- Clients can implement input sequence validation (field exists but not used)

**Duplication:**
- Duplicate states are harmless (just overwrite previous state)
- Duplicate inputs may cause unintended movement (server could deduplicate by sequence)

### When Reliability Is Needed

Use TCP or implement application-level acknowledgment for:
- Player authentication tokens
- Room/lobby operations
- Chat messages
- Score submission to backend services

**Current Protocol:** Only initial TCP handshake is reliable. All gameplay is UDP.

## Security Model

### Current State: ⚠️ **INSECURE**

This protocol is designed for:
- Trusted local networks
- Educational projects
- Prototyping and game jams

**Known Vulnerabilities:**

1. **No Authentication:** Any client can claim any UDP port and receive state
2. **Token Exposure:** Authentication token sent in plaintext over UDP
3. **Spoofing:** Attacker can send inputs claiming to be another player
4. **Eavesdropping:** All game data visible to network sniffers
5. **Denial of Service:** No rate limiting or flood protection
6. **Malformed Packets:** Limited validation of packet contents

### Hardening Recommendations (Future)

For production deployment, consider:

1. **Cryptographic Token:** Use HMAC or JWT for UDP session validation
2. **Encryption:** TLS for TCP, DTLS for UDP
3. **Rate Limiting:** Limit input messages per second per client
4. **Validation:** Strictly validate all size fields and entity counts
5. **Sequence Numbers:** Reject out-of-order or duplicate inputs
6. **Timeout:** Remove inactive clients after N seconds without messages
7. **Checksum:** Add CRC32 or other integrity check to each message

## Error Handling

### Server Behavior

**Invalid Version:**
- Action: Silently ignore message
- Rationale: May be old client or packet corruption

**Undersized Packet:**
- Action: Ignore if < HeaderSize, otherwise try to parse
- Rationale: Truncated packets are useless

**Oversized Payload:**
- Action: Parse what fits, ignore excess
- Current: `header.size` field not strictly validated

**Unknown MsgType:**
- Action: Silently ignore
- Rationale: May be from newer protocol version

### Recommended Client Behavior

**No State Received (Timeout):**
- After 5 seconds: Show "Connection Lost" warning
- After 10 seconds: Return to menu or attempt reconnect

**Invalid State (Entity Count Mismatch):**
- Validate: `payload_size >= 2 + (header.count * 25)`
- If invalid: Skip this state, wait for next valid state
- Log: Warning for debugging

**Unknown MsgType:**
- Ignore message
- Optional: Log for debugging

## Extensibility

### Adding New Message Types

1. Add to `MsgType` enum in `Protocol.hpp`
2. Define payload structure with `#pragma pack(push, 1)`
3. Document in new `udp-XX-name.md` or `tcp-XXX-name.md` file
4. Update this overview and README.md

**Value Ranges:**
- 1-99: UDP messages
- 100-199: TCP messages
- 200+: Reserved for future use

### Reserved Message Types

The protocol defines several reserved message types for future implementation:

- `Spawn` (5): Delta updates for entity creation
- `Despawn` (6): Delta updates for entity removal
- `Ping` (7): Latency measurement request
- `Pong` (8): Latency measurement response

**Benefits of Delta Updates:**
- Reduced bandwidth when only few entities change
- Current approach: Send all entities every frame (simpler but wasteful)

## Testing and Debugging

### Packet Capture

Use Wireshark or tcpdump to inspect traffic:

```bash
# Capture UDP on port 4242
sudo tcpdump -i any -n udp port 4242 -X

# Or with Wireshark filter
udp.port == 4242
```

### Manual Packet Construction

For testing, you can construct packets manually:

**Hello Message (hex):**
```
00 00 01 01
```
- `00 00`: size = 0 (no payload)
- `01`: type = Hello
- `01`: version = 1

**Input Message (sequence=0, move right):**
```
05 00 03 01 00 00 00 00 08
```
- `05 00`: size = 5
- `03`: type = Input
- `01`: version = 1
- `00 00 00 00`: sequence = 0
- `08`: bits = InputRight (1 << 3)

### Common Issues

**Client not receiving state:**
- Verify UDP Hello was sent with correct token
- Check server received Hello (server logs)
- Verify no firewall blocking UDP

**Incorrect entity positions:**
- Verify endianness matches (little-endian)
- Check struct packing (`#pragma pack`)
- Validate float parsing (IEEE-754)

**Server not responding to input:**
- Ensure payload size >= 5 bytes
- Verify version field = 1
- Check input bits are non-zero

## References

- **Protocol Header:** [01-header.md](01-header.md)
- **Transport Layer:** [02-transport.md](02-transport.md)
- **Data Structures:** [03-data-structures.md](03-data-structures.md)
- **Source Code:** `common/include/common/Protocol.hpp`

---

**Last Updated:** 2025-10-29

