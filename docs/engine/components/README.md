## Components

This folder documents each ECS component. Components are pure data; they do not contain behavior.

- `Position`
- `Velocity`
- `Size`
- `Controller`
- `AiController`
- `Player`
- `Enemy`
- `Collided`

```mermaid
classDiagram
  class Position {
    x: float
    y: float
  }
  class Velocity {
    dx: float
    dy: float
  }
  class Size {
    w: float
    h: float
  }
  class Controller {
    inputs
  }
  class AiController {
    state
  }
  class Player {
    id
    lives
  }
  class Enemy {
    type
  }
  class Collided {
    withEntityId
  }
```


