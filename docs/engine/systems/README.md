## Systems

This folder documents each ECS system. Systems iterate over entities with specific component signatures and update their data.

- `MovementSystem`
- `CollisionSystem`
- `PlayerControlSystem`
- `AiControlSystem`

```mermaid
flowchart LR
  PlayerControlSystem --> Velocity
  AiControlSystem --> Velocity
  MovementSystem --> Position
  CollisionSystem --> Collided

  Velocity --> MovementSystem
  Position --> CollisionSystem
  Size --> CollisionSystem
```


