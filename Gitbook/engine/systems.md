---
icon: activity
---

# Systems

Summary of main systems and responsibilities. See ECS Graph for dependencies and tick order.

- InputSystem: apply PlayerInput bits to player movement.
- ShootingSystem: spawn small bullets while shoot is held in normal mode.
- ChargeShootingSystem: accumulate charge while held and spawn a beam on release.
- FormationSystem: update formation origins and follower positions (snake, line, grid, triangle).
- MovementSystem: integrate velocity for all entities.
- EnemyShootingSystem: target players and fire enemy bullets with configurable accuracy.
- DespawnOffscreenSystem: remove entities that leave the world to the left.
- DespawnOutOfBoundsSystem: remove bullets outside the visible area.
- CollisionSystem: resolve bullet hits, destroy enemies, award score, mark player hits.
- InvincibilitySystem: tick down temporary invulnerability.
- FormationSpawnSystem: spawn enemy formations periodically with varied params.
