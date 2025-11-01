## Engine Documentation

This section documents the game engine architecture and its ECS (Entity-Component-System) runtime. It mirrors the structure of the protocol docs: a base overview and subfolders for components and systems.

- See `00-overview.md` for the high-level design and lifecycle
- See `components/` for individual component pages
- See `systems/` for individual system pages

```mermaid
flowchart LR
  subgraph ECS
    E[Entity] -- has --> C[Component]
    S[System] -- reads/writes --> C
    S -. iterates .-> E
  end

  subgraph Components
    Position & Velocity & Size & Controller & AiController & Player & Enemy & Collided
  end

  S --> Position
  S --> Velocity
  S --> Size
  S --> Controller
  S --> AiController
  S --> Player
  S --> Enemy
  S --> Collided
```


