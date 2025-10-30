## Engine Overview

The engine uses an ECS (Entity-Component-System) model:

- Entities are lightweight IDs
- Components store pure data attached to entities
- Systems iterate entities matching required components and update state each frame

### Runtime lifecycle

```mermaid
sequenceDiagram
  participant App as Application
  participant Reg as Registry
  participant Sys as Systems

  App->>Reg: initialize registry and components
  App->>Sys: register systems (Movement, Collision, PlayerControl, AiControl)
  loop Game Loop (tick)
    App->>Sys: run frame(timestep)
    Sys->>Reg: query entities with component signatures
    Sys->>Reg: read/write component data
    App->>App: render / I/O
  end
```

### Component -> System relationships

```mermaid
flowchart TB
  subgraph Components
    Position
    Velocity
    Size
    Controller
    AiController
    Player
    Enemy
    Collided
  end

  subgraph Systems
    MovementSystem
    CollisionSystem
    PlayerControlSystem
    AiControlSystem
  end

  MovementSystem --> Position
  MovementSystem --> Velocity

  CollisionSystem --> Position
  CollisionSystem --> Size
  CollisionSystem --> Collided

  PlayerControlSystem --> Controller
  PlayerControlSystem --> Player
  PlayerControlSystem --> Velocity

  AiControlSystem --> AiController
  AiControlSystem --> Enemy
  AiControlSystem --> Velocity
```


