# ECS Graph — Components and Systems

This file provides a visual overview of the ECS relationships in the engine. It is generated from the current headers and source to help contributors navigate behavior at a glance.

## Tick order

The systems are executed by the server in the following order:

1. InputSystem
2. ShootingSystem
3. ChargeShootingSystem
4. FormationSystem
5. MovementSystem
6. EnemyShootingSystem
7. DespawnOffscreenSystem
8. DespawnOutOfBoundsSystem
9. CollisionSystem
10. InvincibilitySystem
11. FormationSpawnSystem


## Systems ↔ Components dependency graph (Mermaid)

```mermaid
flowchart LR
  subgraph Components
    Transform
    Velocity
    ColorRGBA
    NetType
    Size
    PlayerInput
    EnemyTag
    BulletTag
    BulletOwner
    Shooter
    ChargeGun
    EnemyShooter
    Score
    HitFlag
    Invincible
    Formation
    FormationFollower
    BeamTag
  end

  subgraph Systems
    InputSystem
    ShootingSystem
    ChargeShootingSystem
    FormationSystem
    MovementSystem
    EnemyShootingSystem
    DespawnOffscreenSystem
    DespawnOutOfBoundsSystem
    CollisionSystem
    InvincibilitySystem
    FormationSpawnSystem
  end

  %% InputSystem: reads PlayerInput, writes Transform
  PlayerInput --> InputSystem
  InputSystem --> Transform

  %% ShootingSystem: reads PlayerInput/Shooter/Transform, creates Bullet(Transform, Velocity, NetType, ColorRGBA, BulletTag, BulletOwner, Size)
  PlayerInput --> ShootingSystem
  Shooter --> ShootingSystem
  Transform --> ShootingSystem
  ShootingSystem -->|create| Transform
  ShootingSystem -->|create| Velocity
  ShootingSystem -->|create| NetType
  ShootingSystem -->|create| ColorRGBA
  ShootingSystem -->|create| BulletTag
  ShootingSystem -->|create| BulletOwner
  ShootingSystem -->|create| Size

  %% ChargeShootingSystem: reads PlayerInput/ChargeGun/Transform, creates Beam (Transform, Velocity, NetType, ColorRGBA, BulletTag, BulletOwner, Size, BeamTag)
  PlayerInput --> ChargeShootingSystem
  ChargeGun --> ChargeShootingSystem
  Transform --> ChargeShootingSystem
  ChargeShootingSystem -->|create| Transform
  ChargeShootingSystem -->|create| Velocity
  ChargeShootingSystem -->|create| NetType
  ChargeShootingSystem -->|create| ColorRGBA
  ChargeShootingSystem -->|create| BulletTag
  ChargeShootingSystem -->|create| BulletOwner
  ChargeShootingSystem -->|create| Size
  ChargeShootingSystem -->|create| BeamTag

  %% FormationSystem: reads Formation, FormationFollower, Transform, Velocity; writes Transform, Velocity
  Formation --> FormationSystem
  FormationFollower --> FormationSystem
  Transform --> FormationSystem
  Velocity --> FormationSystem
  FormationSystem --> Transform
  FormationSystem --> Velocity

  %% MovementSystem: reads Velocity, writes Transform
  Velocity --> MovementSystem
  MovementSystem --> Transform

  %% EnemyShootingSystem: reads EnemyShooter, Transform, NetType (to find players), creates enemy Bullet(Transform, Velocity, NetType, ColorRGBA, BulletTag, Size)
  EnemyShooter --> EnemyShootingSystem
  Transform --> EnemyShootingSystem
  NetType --> EnemyShootingSystem
  EnemyShootingSystem -->|create| Transform
  EnemyShootingSystem -->|create| Velocity
  EnemyShootingSystem -->|create| NetType
  EnemyShootingSystem -->|create| ColorRGBA
  EnemyShootingSystem -->|create| BulletTag
  EnemyShootingSystem -->|create| Size

  %% DespawnOffscreenSystem: reads Transform, destroys entities with x < minX
  Transform --> DespawnOffscreenSystem
  DespawnOffscreenSystem -->|destroy| Transform

  %% DespawnOutOfBoundsSystem: reads Transform, Size, BulletTag; destroys bullets outside bounds
  Transform --> DespawnOutOfBoundsSystem
  Size --> DespawnOutOfBoundsSystem
  BulletTag --> DespawnOutOfBoundsSystem
  DespawnOutOfBoundsSystem -->|destroy| BulletTag

  %% CollisionSystem: reads BulletTag, BeamTag, Transform, Size, EnemyTag, PlayerInput, Score, BulletOwner, Invincible; destroys on hits and updates Score/HitFlag/Invincible
  BulletTag --> CollisionSystem
  BeamTag --> CollisionSystem
  Transform --> CollisionSystem
  Size --> CollisionSystem
  EnemyTag --> CollisionSystem
  PlayerInput --> CollisionSystem
  Score --> CollisionSystem
  BulletOwner --> CollisionSystem
  Invincible --> CollisionSystem
  CollisionSystem -->|destroy| EnemyTag
  CollisionSystem -->|destroy| BulletTag
  CollisionSystem --> Score
  CollisionSystem --> HitFlag
  CollisionSystem --> Invincible

  %% InvincibilitySystem: reads/writes Invincible
  Invincible --> InvincibilitySystem
  InvincibilitySystem --> Invincible

  %% FormationSpawnSystem: creates Formation origins and Enemy followers with multiple components
  FormationSpawnSystem -->|create| Transform
  FormationSpawnSystem -->|create| Velocity
  FormationSpawnSystem -->|create| Formation
  FormationSpawnSystem -->|create| FormationFollower
  FormationSpawnSystem -->|create| NetType
  FormationSpawnSystem -->|create| ColorRGBA
  FormationSpawnSystem -->|create| EnemyTag
  FormationSpawnSystem -->|create| Size
  FormationSpawnSystem -->|create| EnemyShooter
```

## Legend

- A --> System: component is read by the system.
- System --> Component: component is written/updated by the system.
- System -->|create| Component: entities with these components are created by the system.
- System -->|destroy| Component: entities possessing these components may be destroyed by the system.

Notes:
- NetType is used by EnemyShootingSystem to find players and by bullet spawners to categorize new entities.
- CollisionSystem awards Score to BulletOwner on enemy kill and marks players with HitFlag and Invincible.
- FormationSystem clamps follower positions to the playable vertical band.
