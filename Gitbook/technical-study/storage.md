# Storage Study

This document analyzes storage techniques used in the R-Type project, covering persistence, reliability, and storage constraints.

## Overview

The R-Type project is primarily a real-time multiplayer game with **no persistent storage requirements** in its current implementation. However, this study analyzes the storage techniques used for in-memory data management and discusses potential future persistence needs.

## Storage Categories

### 1. In-Memory Game State

**Storage Type:** Volatile (RAM only)

**Data Stored:**
- Entity components (Position, Velocity, etc.)
- Player input states
- Network connection state
- Current game world state

**Characteristics:**
- **Lifetime:** Exists only during game session
- **Persistence:** None (resets on server restart)
- **Reliability:** Single point of failure (server crash = data loss)
- **Performance:** Extremely fast (RAM access)

**Storage Mechanisms:**

#### Component Storage
```cpp
// Sparse storage pattern
std::vector<T> dense_;                           // Component data
std::unordered_map<EntityId, size_t> sparse_;     // Entity -> index
```

**Reliability:** 
- No persistence layer
- Data lost on server crash
- Acceptable for game session state

**Constraints:**
- Memory usage: O(n) where n = number of components
- Max entities limited by available RAM
- Current limit: 512 entities (protocol constraint)

### 2. Network State (Client-Side)

**Storage Type:** Volatile (client RAM)

**Data Stored:**
- Latest received world state
- Input history (for potential replay)
- Connection state

**Reliability:**
- No persistence required
- Client disconnection = state reset
- Reconnect gets fresh state from server

## Storage Techniques Analysis

### Technique 1: In-Memory Only (Current)

**Description:** All game state stored in RAM, no persistence

**Pros:**
- **Performance:** Fastest possible access
- **Simplicity:** No I/O operations, no file system dependencies
- **Consistency:** No synchronization issues
- **Suitable for:** Session-based games, real-time multiplayer

**Cons:**
- **Volatility:** Data lost on crash
- **No History:** Cannot recover or replay games
- **No Progression:** Player stats, achievements, etc. not saved

**Use Case:** Current implementation - appropriate for prototype/session-based gameplay

### Technique 2: File-Based Persistence (Future)

**Potential Implementation:** Save game state to files

**Format Options:**

#### JSON
```json
{
  "entities": [
    {"id": 1, "type": "Player", "x": 100, "y": 200}
  ],
  "players": [
    {"id": 1, "name": "Alice", "score": 1500, "lives": 3}
  ]
}
```

**Pros:**
- Human-readable
- Easy to debug
- Flexible schema

**Cons:**
- Large file size
- Slow parsing
- Not suitable for real-time

#### Binary Format
```cpp
// Custom binary format (similar to network protocol)
struct SaveGameHeader {
    std::uint32_t version;
    std::uint32_t entity_count;
    // ... packed entities
};
```

**Pros:**
- Compact
- Fast read/write
- Similar to network protocol

**Cons:**
- Not human-readable
- Version-dependent
- Requires migration tools

#### SQLite Database
```sql
CREATE TABLE entities (
    id INTEGER PRIMARY KEY,
    type INTEGER,
    x REAL, y REAL,
    -- ...
);
```

**Pros:**
- Structured queries
- ACID guarantees
- Standard format

**Cons:**
- Overhead for simple use case
- Requires SQLite dependency
- More complex than needed

**Recommendation:** If persistence needed, use binary format similar to network protocol for consistency and performance.

### Technique 3: Database Storage (Not Used)

**Potential Use Cases:**
- Player profiles and statistics
- Leaderboards
- Match history
- Persistent progression

**Options:**

| Database | Pros | Cons | Recommendation |
|----------|------|------|----------------|
| **PostgreSQL** | ACID, powerful queries, open source | Heavy for game server | For large-scale production |
| **MySQL/MariaDB** | Common, well-supported | Less features than PostgreSQL | Good alternative |
| **SQLite** | File-based, no server needed | Single-writer limitation | For simple stats only |
| **Redis** | In-memory, extremely fast | Volatile (needs persistence) | For caching, not primary storage |
| **MongoDB** | Flexible schema, JSON-like | No ACID (until recent versions) | For unstructured data |

**Current Status:** Not implemented - no persistent storage needed for prototype

**Future Consideration:** If adding features like:
- Player accounts
- Leaderboards
- Match replays
- Statistics tracking

Then implement database layer with PostgreSQL or SQLite depending on scale.

### Technique 4: Configuration Storage

**Current:** Hard-coded or command-line arguments

**Future Options:**

#### INI Files
```ini
[server]
port=4242
max_players=4
tick_rate=60
```

#### JSON Configuration
```json
{
  "server": {
    "port": 4242,
    "max_players": 4,
    "tick_rate": 60
  }
}
```

#### YAML Configuration
```yaml
server:
  port: 4242
  max_players: 4
  tick_rate: 60
```

**Recommendation:** JSON or YAML for readability, or simple binary format for performance.

## Reliability Analysis

### Current Implementation

**Reliability Level:** **Low** (intentional for prototype)

**Characteristics:**
- No persistence layer
- No crash recovery
- No state backup
- Single server instance

**Impact:**
- Server crash = game state lost
- Client disconnect = must reconnect from scratch
- No replay capability

**Acceptability:**
- ✅ Appropriate for prototype/session-based game
- ✅ Matches project requirements (no persistence mentioned)
- ❌ Not suitable for production with player accounts/stats

### Reliability Improvements (Future)

#### 1. State Snapshots

**Technique:** Periodically save complete game state

**Implementation:**
```cpp
void saveSnapshot(Registry& reg, const std::string& path) {
    // Serialize all entities and components
    // Write to file every N seconds
}
```

**Benefits:**
- Can recover from crash (restore last snapshot)
- Enables replay functionality
- Debugging tool

**Drawbacks:**
- I/O overhead during gameplay
- Requires state serialization

#### 2. Write-Ahead Logging

**Technique:** Log all state changes before applying

**Implementation:**
```cpp
struct StateChange {
    EntityId entity;
    ComponentType type;
    Timestamp time;
    // Change data
};
```

**Benefits:**
- Complete history of changes
- Can replay from any point
- Debugging and analysis

**Drawbacks:**
- Significant storage overhead
- Performance impact (logging every change)

#### 3. Database Transactions

**Technique:** Use ACID-compliant database for critical data

**Use Cases:**
- Player scores
- Match results
- Statistics

**Benefits:**
- Data integrity guaranteed
- Recovery from crashes
- Concurrent access safe

**Drawbacks:**
- Database dependency
- Performance overhead
- Overkill for game state

## Storage Constraints

### Memory Constraints

**Current Limits:**
- Max entities: 512 (protocol limit)
- Component storage: Limited by available RAM
- Network buffers: 2048 bytes per packet

**Memory Usage Estimation:**
```
Per entity (average):
- Position: 8 bytes (2 floats)
- Velocity: 8 bytes (2 floats)
- Size: 8 bytes (2 floats)
- Other components: ~20 bytes
Total: ~44 bytes per entity

512 entities × 44 bytes = ~22 KB
+ Overhead (maps, vectors) = ~50-100 KB total
```

**Conclusion:** Memory usage is minimal, not a constraint.

### Disk Storage Constraints

**Current:** None (no persistence)

**If Persistence Added:**
- Save game: ~100 KB per game
- Match replay: ~1 MB per match (if logging all state)
- Player stats: ~1 KB per player

**Conclusion:** Storage requirements would be minimal even with persistence.

### Network Storage Constraints

**Protocol Limits:**
- UDP packet size: 65,507 bytes (max)
- Practical limit: ~1400 bytes (to avoid fragmentation)
- Current State message: Up to ~12,806 bytes (512 entities × 25 bytes + header)

**Strategy:**
- Prioritize entities (players first)
- Truncate if exceeds limit
- Accept one-frame visual loss for low-priority entities

## Recommendations

### For Current Prototype

✅ **Current approach is appropriate:**
- In-memory storage sufficient
- No persistence needed
- Matches project requirements

### For Production Enhancement

**If adding features requiring persistence:**

1. **Player Statistics:**
   - Use SQLite for simple, file-based storage
   - Store player scores, match history
   - No server required

2. **Match Replays:**
   - Save state snapshots every N frames
   - Use binary format (similar to network protocol)
   - Compress with zlib/gzip for storage efficiency

3. **Leaderboards:**
   - Use PostgreSQL for structured queries
   - Index on score columns for fast ranking
   - Cache frequently accessed leaderboards in Redis

4. **Configuration:**
   - Use JSON or YAML for server configuration
   - Allow runtime reload without restart

## Conclusion

The current storage approach (in-memory only) is:
- ✅ Appropriate for prototype scope
- ✅ High performance (no I/O overhead)
- ✅ Simple to implement
- ✅ Meets project requirements

Future storage needs can be addressed incrementally based on feature requirements, using appropriate techniques for each use case (database for stats, file-based for config, binary for replays).
