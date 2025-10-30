# Transport Layer

## Overview

The R-Type protocol uses a **dual-transport architecture**: TCP for initial connection setup and UDP for all gameplay traffic. This design provides the reliability needed for authentication while maintaining the low latency required for real-time gameplay.

## Transport Selection

### TCP Usage

**Purpose:** Reliable, ordered delivery for connection establishment

**Used For:**
- Initial client-server handshake
- Authentication token exchange
- UDP port discovery
- (Future) Lobby/room management
- (Future) Chat messages

**Messages:**
- `TcpWelcome` (100)
- `StartGame` (101)

**Connection Lifecycle:**
1. Client connects to server TCP port (default: 4242)
2. Server sends `TcpWelcome` with UDP port
3. Server sends `StartGame` with authentication token
4. Client **may** close TCP connection after receiving token
5. (Optional) Keep TCP open for lobby features

**Default TCP Port:** Same as UDP (4242), but configurable independently

### UDP Usage

**Purpose:** Low-latency, connectionless transport for real-time gameplay

**Used For:**
- All gameplay traffic
- Player input commands
- World state synchronization
- Game events (spawn, score, lives)
- Session management during gameplay

**Messages:**
- All message types 1-99 (Hello, Input, State, etc.)

**Properties:**
- **No guaranteed delivery:** Packets may be lost
- **No ordering:** Packets may arrive out of order
- **No congestion control:** Application must manage send rate
- **Lightweight:** Minimal protocol overhead (8 bytes UDP header + 4 bytes protocol header)

**Default UDP Port:** Provided by server in `TcpWelcome` message (typically 4242)

## Connection Flow

### Complete Connection Sequence

```
Phase 1: TCP Handshake
─────────────────────────

Client                                Server
   │                                     │
   │───── TCP SYN ───────────────────────>│
   │<──── TCP SYN-ACK ────────────────────│
   │───── TCP ACK ───────────────────────>│
   │                                     │
   │                                     │
   │<──── TcpWelcome (UDP port) ──────────│
   │                                     │
   │<──── StartGame (token) ──────────────│
   │                                     │
   │───── TCP FIN (optional) ────────────>│
   │                                     │


Phase 2: UDP Session Establishment
───────────────────────────────────

Client                                Server
   │                                     │
   │───── UDP Hello (token) ──────────────>│
   │                                     │
   │                [Server validates]    │
   │                [Creates player]      │
   │                                     │
   │<──── UDP HelloAck ────────────────────│
   │                                     │
   │<──── State (includes player) ─────────│
   │<──── Roster (player list) ────────────│
   │                                     │


Phase 3: Active Gameplay
─────────────────────────

Client                                Server
   │                                     │
   │───── Input (60 Hz) ───────────────────>│
   │<──── State (60 Hz) ────────────────────│
   │                                     │
   │<──── LivesUpdate (on event) ───────────│
   │<──── ScoreUpdate (on event) ───────────│
   │                                     │
   │                                     │
   │───── Disconnect (optional) ────────────>│
   │                                     │
```

## TCP Connection Details

### Server TCP Socket

**Configuration:**
```cpp
// Pseudo-code
tcp_socket.bind(endpoint(tcp::v4(), server_port));
tcp_socket.listen();
```

**Accept Loop:**
- Server accepts TCP connections
- Creates session per client
- Sends `TcpWelcome` and `StartGame`
- (Current implementation may close connection after token exchange)

### Client TCP Connection

**Connection Steps:**

1. **Resolve Server Address:**
   ```cpp
   tcp::resolver resolver(io_context);
   auto endpoints = resolver.resolve(server_host, server_port);
   ```

2. **Connect:**
   ```cpp
   tcp::socket socket(io_context);
   asio::connect(socket, endpoints);
   ```

3. **Receive Messages:**
   - Read `TcpWelcome` message (blocking or async)
   - Extract UDP port from payload
   - Read `StartGame` message
   - Extract authentication token from payload

4. **Close or Keep Alive:**
   ```cpp
   // Option A: Close after receiving token
   socket.close();

   // Option B: Keep open for future TCP messages (lobby, chat)
   // Keep socket alive, set up async read loop
   ```

### TCP Message Framing

TCP is a stream protocol, so messages must be framed:

**Recommended Approach:**
1. Read 4-byte header (blocking or async)
2. Parse `size` field from header
3. Read exactly `size` bytes of payload
4. Parse message based on `type` field

**Example (synchronous):**
```cpp
Header header;
asio::read(socket, asio::buffer(&header, sizeof(Header)));

std::vector<uint8_t> payload(header.size);
if (header.size > 0) {
    asio::read(socket, asio::buffer(payload));
}

// Process message based on header.type
```

**Important:** Unlike UDP where each datagram is a complete message, TCP requires careful framing to know where one message ends and the next begins.

## UDP Connection Details

### Server UDP Socket

**Configuration:**
```cpp
// Pseudo-code
udp::socket socket(io_context, udp::endpoint(udp::v4(), udp_port));
```

**Receive Loop:**
```cpp
while (running) {
    udp::endpoint sender_endpoint;
    size_t length = socket.receive_from(
        asio::buffer(recv_buffer),
        sender_endpoint
    );

    handlePacket(recv_buffer.data(), length, sender_endpoint);
}
```

**Send (Broadcast State):**
```cpp
for (auto& [endpoint, player_id] : clients) {
    socket.send_to(asio::buffer(state_packet), endpoint);
}
```

**Client Tracking:**
- Server associates each `udp::endpoint` with a player ID
- Endpoint = (IP address, UDP port)
- First `Hello` from an endpoint creates a new player
- Subsequent messages from same endpoint update that player

### Client UDP Socket

**Socket Creation:**
```cpp
udp::socket socket(io_context);
socket.open(udp::v4());

// Optionally bind to specific local port (usually not needed)
// socket.bind(udp::endpoint(udp::v4(), 0)); // 0 = any port
```

**Send Input:**
```cpp
udp::endpoint server_endpoint(
    address::from_string(server_ip),
    udp_port  // From TcpWelcome
);

socket.send_to(asio::buffer(input_packet), server_endpoint);
```

**Receive State:**
```cpp
while (running) {
    udp::endpoint sender_endpoint;
    size_t length = socket.receive_from(
        asio::buffer(recv_buffer),
        sender_endpoint
    );

    // Verify sender is our server (optional but recommended)
    if (sender_endpoint != server_endpoint) {
        continue;  // Ignore packets from other sources
    }

    handlePacket(recv_buffer.data(), length);
}
```

### UDP Message Framing

**Key Difference from TCP:** Each UDP datagram is exactly one complete message.

**Receive Pattern:**
```cpp
uint8_t buffer[2048];  // Must be large enough for largest message
size_t bytes_received = socket.receive_from(buffer, sender);

// buffer[0..bytes_received-1] contains exactly one complete message
// Parse header, then payload
```

**No fragmentation handling needed at application level:**
- If message > MTU (~1500 bytes), IP layer handles fragmentation
- If any fragment is lost, entire message is lost
- Application just receives complete message or nothing

## Port Configuration

### Default Ports

**Compile-time Default:**
```cpp
// server/main.cpp
constexpr uint16_t DefaultPort = 4242;
```

**Command-line Override:**
```bash
# Start server on custom port
./r-type_server 5000
```

### Port Usage Patterns

**Pattern 1: Same Port for TCP and UDP (Default)**
- TCP listener on port 4242
- UDP socket on port 4242
- `TcpWelcome` sends `udp_port = 4242`
- Simple configuration

**Pattern 2: Different Ports**
- TCP listener on port 4242
- UDP socket on port 4243
- `TcpWelcome` sends `udp_port = 4243`
- Useful if firewall rules differ for TCP vs UDP

**Pattern 3: Dynamic UDP Port**
- TCP listener on port 4242
- UDP socket on OS-assigned port (bind to port 0)
- `TcpWelcome` sends actual bound port
- Useful when multiple server instances run on same host

### Firewall Configuration

**Server Requirements:**

For default configuration (port 4242):
```bash
# Linux (iptables)
sudo iptables -A INPUT -p tcp --dport 4242 -j ACCEPT
sudo iptables -A INPUT -p udp --dport 4242 -j ACCEPT

# Linux (firewalld)
sudo firewall-cmd --add-port=4242/tcp --permanent
sudo firewall-cmd --add-port=4242/udp --permanent
sudo firewall-cmd --reload

# Windows (PowerShell)
New-NetFirewallRule -DisplayName "RType TCP" -Direction Inbound -Protocol TCP -LocalPort 4242 -Action Allow
New-NetFirewallRule -DisplayName "RType UDP" -Direction Inbound -Protocol UDP -LocalPort 4242 -Action Allow
```

**Client Requirements:**
- Outbound TCP to server port (usually allowed by default)
- Outbound UDP to server port (usually allowed by default)
- Inbound UDP from server (for state messages)

## Network Architecture Considerations

### NAT Traversal

**Current Protocol:** Does **not** handle NAT automatically

**Problem:**
- Client behind NAT sends UDP packet to server
- Server sees client's public IP and NAT-assigned port
- Server sends state to this public IP:port
- NAT router should forward packet to client (if mapping exists)

**Status:** Works for most home routers (full cone or restricted cone NAT)

**Fails for:** Symmetric NAT (each destination gets different source port)

**Solution (if needed in future):**
- Implement STUN-like mechanism
- Use hole-punching technique
- Or require server to be on public IP (current assumption)

### IPv6 Support

**Current Status:** IPv4 only (`tcp::v4()`, `udp::v4()`)

**To Add IPv6:**
1. Change socket initialization to `tcp::v6()`, `udp::v6()`
2. Consider dual-stack (v4 + v6) or separate sockets
3. Update `TcpWelcome` to send both v4 and v6 addresses
4. Update protocol documentation

### Multicast / Broadcast

**Current:** Server sends individual packets to each client (unicast)

**Optimization (future):**
- Use UDP multicast for State messages
- All clients join multicast group
- Server sends one State packet to group
- **Benefit:** Reduced server bandwidth (N clients = 1 packet instead of N packets)
- **Drawback:** Requires multicast-capable network, more complex setup

## Performance Tuning

### UDP Socket Buffer Size

**Default:** OS-dependent (typically 64-256 KB)

**Increase for High Traffic:**
```cpp
// Increase receive buffer to 1 MB
udp::socket socket(io_context);
socket.open(udp::v4());

asio::socket_base::receive_buffer_size option(1024 * 1024);
socket.set_option(option);
```

**Why:** Prevents packet loss when OS receive buffer fills up during traffic bursts

**When:** If many clients or high packet rate (100+ Hz)

### TCP Socket Options

**Disable Nagle's Algorithm:**
```cpp
tcp::no_delay option(true);
tcp_socket.set_option(option);
```

**Why:** Nagle delays small packets for up to 200ms to batch them. This is bad for latency-sensitive messages.

**When:** Always enable for game traffic

**Keep-Alive (if TCP kept open):**
```cpp
asio::socket_base::keep_alive option(true);
tcp_socket.set_option(option);
```

**Why:** Detect dead connections

### UDP Send Rate Limiting

**Problem:** Sending too fast can overflow server receive buffer or cause network congestion

**Client Input Rate:**
```cpp
// Option 1: Fixed rate (60 Hz)
auto next_send = now + 16ms;
if (now >= next_send) {
    sendInput();
    next_send += 16ms;
}

// Option 2: Send on change only
if (current_input != last_sent_input) {
    sendInput();
    last_sent_input = current_input;
}

// Option 3: Hybrid (on change, max 60 Hz)
if (current_input != last_sent_input && now >= next_send) {
    sendInput();
    last_sent_input = current_input;
    next_send = now + 16ms;
}
```

**Recommended:** Hybrid approach (send on change, rate-limited)

## Error Handling

### TCP Errors

**Connection Refused:**
- Server not running or port blocked
- Client: Show "Cannot connect to server" error

**Connection Reset:**
- Server crashed or closed connection
- Client: If before UDP session, abort; if after, continue on UDP

**Timeout:**
- Network issue or server unresponsive
- Client: Retry with backoff or abort

### UDP Errors

**ICMP Port Unreachable:**
- Server not listening on UDP port
- Client: Show "Server UDP port not reachable"

**No Response to Hello:**
- Server didn't send HelloAck within timeout (e.g., 5 seconds)
- Client: Retry Hello or abort

**No State Received:**
- Server stopped sending or network issue
- Client: Show "Connection lost" after timeout (e.g., 10 seconds)

### Implementation Example

```cpp
// TCP connection with timeout
tcp::socket socket(io_context);
boost::system::error_code ec;

// Set timeout
auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);

asio::async_connect(socket, endpoints,
    [&](const boost::system::error_code& error, const tcp::endpoint&) {
        if (!error) {
            // Connected
        } else if (error == asio::error::operation_aborted) {
            // Timeout
        } else {
            // Other error
        }
    });

io_context.run_until(deadline);
```

## Security Considerations

### TCP Security

**Current State:**
- No TLS/SSL encryption
- No certificate validation
- Vulnerable to man-in-the-middle attacks

**Hardening:**
- Use Asio SSL/TLS support
- Validate server certificate
- Use strong cipher suites

### UDP Security

**Current State:**
- Token sent in plaintext
- No packet authentication
- Vulnerable to spoofing and eavesdropping

**Hardening:**
- Use DTLS (Datagram TLS) for encryption
- Add HMAC to each packet for authentication
- Use cryptographically strong tokens (128+ bits)

### Denial of Service

**TCP SYN Flood:**
- Attacker sends many SYN packets, exhausting server resources
- Mitigation: OS-level SYN cookies, rate limiting

**UDP Flood:**
- Attacker sends many packets, consuming server bandwidth
- Mitigation: Rate limit per source IP, validate tokens early

## Testing Transport Layer

### Local Testing

**Start Server:**
```bash
./r-type_server 4242
```

**Start Client:**
```bash
./r-type_client localhost 4242
```

**Verify TCP:**
```bash
netstat -an | grep 4242
# Should show ESTABLISHED connection
```

**Verify UDP:**
```bash
sudo tcpdump -i lo udp port 4242 -v
# Should show UDP packets flowing
```

### Network Testing

**Simulate Packet Loss:**
```bash
# Linux: Add 10% packet loss on UDP
sudo tc qdisc add dev eth0 root netem loss 10%

# Remove after testing
sudo tc qdisc del dev eth0 root
```

**Simulate Latency:**
```bash
# Add 100ms latency
sudo tc qdisc add dev eth0 root netem delay 100ms

# Remove
sudo tc qdisc del dev eth0 root
```

**Bandwidth Limiting:**
```bash
# Limit to 1 Mbps
sudo tc qdisc add dev eth0 root tbf rate 1mbit burst 32kbit latency 400ms
```

## Related Documentation

- **[00-overview.md](00-overview.md)** - Protocol overview
- **[01-header.md](01-header.md)** - Message header format
- **[tcp-100-welcome.md](tcp-100-welcome.md)** - TcpWelcome message
- **[tcp-101-start-game.md](tcp-101-start-game.md)** - StartGame message
- **[udp-01-hello.md](udp-01-hello.md)** - UDP Hello message

---

**Last Updated:** 2025-10-29

