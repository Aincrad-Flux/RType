# Algorithms and Data Structures

This document describes the algorithms, data structures, and design patterns selected for the R-Type project, including rationale for existing solutions and custom implementations.

## Architecture Pattern: Entity-Component-System (ECS)

### Choice: ECS Architecture

**Rationale:**
- **Decoupling:** Systems don't depend on specific entity types, only component combinations
- **Composition over Inheritance:** Entities are composed of components, avoiding deep inheritance hierarchies
- **Performance:** Cache-friendly data layouts (components stored contiguously)
- **Flexibility:** Easy to add new features by adding components/systems without modifying existing code

**Alternatives Considered:**

| Pattern | Pros | Cons | Decision |
|---------|------|------|----------|
| **Traditional OOP** | Familiar, straightforward | Tight coupling, hard to extend | Not chosen - less flexible |
| **Data-Oriented Design** | Maximum performance | Very low-level, complex | Not chosen - ECS provides balance |
| **Actor Model** | Good for concurrency | Overkill for game engine | Not chosen - wrong abstraction |

### Implementation Details

**Registry Pattern:**
```cpp
// Central registry manages all entities and components
class Registry {
    std::unordered_map<EntityId, ComponentMask> entities_;
    std::unordered_map<ComponentTypeId, std::unique_ptr<ComponentStorage>> storages_;
};
```

**Component Storage:**
- **Sparse Sets:** Used for component storage to minimize memory waste
- **Type Erasure:** Components stored using type-erased storage with type-safe retrieval
- **Archetype-based:** Could be extended to archetype optimization (future enhancement)

## Data Structures

### 1. Sparse Component Storage

**Choice:** Sparse array/vector pattern for components

**Rationale:**
- **Memory Efficiency:** Only store components for entities that have them
- **Cache Locality:** Components of same type stored contiguously
- **Fast Iteration:** Iterate over components without checking every entity

**Structure:**
```cpp
template<typename T>
class ComponentStorage {
    std::vector<T> dense_;           // Actual component data
    std::unordered_map<EntityId, size_t> sparse_;  // Entity -> index mapping
};
```

**Time Complexity:**
- Insert: O(1) average case
- Lookup: O(1) average case
- Iteration: O(n) where n = number of components of type T

### 2. Entity ID Management

**Choice:** Monotonically increasing 32-bit integer with generation counter

**Rationale:**
- **Simple:** Easy to validate and track
- **Unique:** Each entity gets unique ID
- **Lightweight:** 32-bit integer is sufficient for game scope

**Structure:**
```cpp
using EntityId = std::uint32_t;
// Current implementation uses sequential IDs
// Future: Could add generation counter for reuse safety
```

### 3. Input Bitmask

**Choice:** Single-byte bitmask for player input

**Rationale:**
- **Compact:** 8 bits encode all input states
- **Efficient Network:** Minimizes packet size (1 byte vs multiple booleans)
- **Fast Processing:** Bitwise operations are extremely fast

**Structure:**
```cpp
enum : std::uint8_t {
    InputUp     = 1 << 0,  // 0x01
    InputDown   = 1 << 1,  // 0x02
    InputLeft   = 1 << 2,  // 0x04
    InputRight  = 1 << 3,  // 0x08
    InputShoot  = 1 << 4,  // 0x10
    InputCharge = 1 << 5,  // 0x20
};
```

**Alternatives:**
- **Separate Booleans:** Would require 6 bytes vs 1 byte
- **Enum Values:** Less flexible for multiple simultaneous inputs
- **JSON/Text:** Way too verbose for network packets

### 4. Packed Binary Protocol Structures

**Choice:** `#pragma pack(push, 1)` for network payloads

**Rationale:**
- **Deterministic Layout:** Ensures consistent binary format across platforms
- **No Padding:** Eliminates padding bytes that waste bandwidth
- **Direct Serialization:** Can memcpy directly to/from network buffers

**Structure Example:**
```cpp
#pragma pack(push, 1)
struct PackedEntity {
    std::uint32_t id;
    EntityType type;
    float x, y;
    float vx, vy;
    std::uint32_t rgba;
};  // Exactly 25 bytes
#pragma pack(pop)
```

**Trade-off:**
- **Pros:** Optimal bandwidth usage, fast serialization
- **Cons:** Platform-dependent (little-endian only), potential alignment issues
- **Mitigation:** Documented endianness requirement, validate on both ends

## Algorithms

### 1. Entity Query/Iteration

**Algorithm:** Iterate entities matching component signature

**Implementation:**
```cpp
template<typename... Components>
void forEach(Registry& reg, std::function<void(EntityId, Components&...)> func) {
    // 1. Find intersection of entities with all required components
    // 2. Iterate matching entities
    // 3. Invoke function with entity ID and component references
}
```

**Time Complexity:** O(n) where n = number of matching entities
**Space Complexity:** O(m) where m = number of component types (for intersection)

**Optimization Opportunities:**
- **Archetypes:** Group entities by component combination (future)
- **Parallel Iteration:** Process entities in parallel (future)

### 2. Collision Detection

**Algorithm:** AABB (Axis-Aligned Bounding Box) collision detection

**Rationale:**
- **Simplicity:** Fast to implement and understand
- **Performance:** O(n²) brute force acceptable for small entity counts
- **Sufficient:** AABB works well for game sprites (rectangular bounds)

**Implementation:**
```cpp
bool intersects(const Position& pos1, const Size& size1,
                const Position& pos2, const Size& size2) {
    return (pos1.x < pos2.x + size2.w && pos1.x + size1.w > pos2.x &&
            pos1.y < pos2.y + size2.h && pos1.y + size1.h > pos2.y);
}
```

**Time Complexity:** O(n²) for n entities (all pairs checked)
**Optimization Future:** Spatial partitioning (quadtree, grid) for O(n log n) or O(n)

**Alternatives:**
- **Circle Collision:** Simpler but less accurate for rectangular sprites
- **SAT (Separating Axis Theorem):** More accurate but complex for simple game
- **Broad/Narrow Phase:** Overkill for current entity count

### 3. Network Packet Prioritization

**Algorithm:** Priority-based entity packing for State messages

**Rationale:**
- **Bandwidth Constraints:** UDP MTU limits packet size
- **Critical Entities First:** Players most important, then bullets, then enemies
- **Graceful Degradation:** If too many entities, drop low-priority ones

**Implementation:**
```cpp
void packState(Registry& reg, std::vector<PackedEntity>& out) {
    // 1. Collect all entities
    // 2. Sort by priority: Player > Bullet > Enemy
    // 3. Pack until size limit reached
    // 4. Truncate if necessary
}
```

**Priority Order:**
1. Players (critical for gameplay)
2. Bullets (affect gameplay immediately)
3. Enemies (can miss one frame)

## Design Patterns

### 1. System Pattern

**Pattern:** Command/Update pattern for game systems

**Structure:**
```cpp
class System {
public:
    virtual void update(Registry& reg, float dt) = 0;
};
```

**Rationale:**
- **Separation of Concerns:** Each system handles one aspect (movement, collision, etc.)
- **Deterministic Order:** Systems execute in fixed order for consistency
- **Testability:** Systems can be tested independently

### 2. Factory Pattern

**Pattern:** Entity creation through factory functions

**Usage:**
```cpp
EntityId createPlayer(Registry& reg, float x, float y);
EntityId createEnemy(Registry& reg, EnemyType type, float x, float y);
```

**Rationale:**
- **Centralized Creation:** Ensures entities created with correct components
- **Encapsulation:** Hides component setup complexity
- **Consistency:** All players/enemies created identically

### 3. Observer Pattern (Future)

**Pattern:** Event-driven updates (not yet implemented)

**Potential Usage:**
- Collision events trigger score updates
- Player death events trigger respawn logic
- Enemy spawn events notify clients

**Rationale:**
- **Decoupling:** Systems don't need direct references
- **Flexibility:** Easy to add new event handlers

## Custom Algorithms

### 1. Formation Movement Algorithm

**Purpose:** Move enemies in coordinated patterns (snake, line, grid, triangle)

**Algorithm:**
```cpp
void updateFormation(FormationOrigin& origin, std::vector<FormationFollower>& followers) {
    // 1. Update origin position (horizontal movement)
    // 2. Calculate follower positions relative to origin
    // 3. Apply pattern-specific offsets (sinusoidal for snake, etc.)
    // 4. Clamp to playable area
}
```

**Why Custom:**
- Game-specific pattern requirements
- Performance: Simpler than general pathfinding
- Deterministic: Must be same on server and client

### 2. Binary Protocol Serialization

**Purpose:** Convert game state to/from binary network format

**Algorithm:**
- Direct memory copy for packed structures
- Manual serialization for variable-length data (player names)
- Endianness handling (current: little-endian only)

**Why Custom:**
- Performance: No serialization library overhead
- Control: Exact byte layout required
- Size: Minimal packet overhead

**Trade-offs:**
- **Pros:** Fast, compact, deterministic
- **Cons:** Platform-dependent, manual maintenance
- **Mitigation:** Comprehensive protocol documentation, versioning

## Performance Considerations

### Memory Management

**Strategy:** Stack allocation where possible, heap for dynamic collections

**Component Storage:**
- Uses `std::vector` (heap) but pre-allocated capacity
- No frequent allocations during gameplay

**Network Buffers:**
- Fixed-size buffers for UDP packets (2048 bytes)
- No dynamic allocation during packet processing

### Cache Locality

**Optimization:**
- Components of same type stored contiguously
- Systems iterate components directly (no indirection through entities)
- Minimize pointer chasing

**Future Improvements:**
- Archetype-based storage (group entities by component combination)
- SoA (Structure of Arrays) for components used together

## Conclusion

The chosen algorithms and data structures prioritize:
1. **Simplicity:** Easy to understand and maintain
2. **Performance:** Acceptable for current game scope
3. **Extensibility:** Can be optimized as game grows
4. **Network Efficiency:** Minimal bandwidth usage

The ECS architecture provides the foundation for decoupled, performant game logic that scales well.
