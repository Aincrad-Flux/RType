# Engine — ECS Architecture, Components, and Systems

This document describes the in-server game engine built on an Entity–Component–System (ECS) architecture. It focuses on the data model and behavior of gameplay, without source code.

## ECS overview

- Entities: unique identifiers representing players, enemies, bullets, formations, and special effects.
- Components: small plain-data records attached to entities to describe state such as transform, velocity, color, size, tags (enemy/bullet), input bits, shooting capability, charge-shot state, scoring, or formation membership.
- Systems: classes that iterate over entities with specific components and update their state each simulation tick (for example, movement, shooting, collision detection). Systems are composed in a defined order to implement the game rules.

The ECS registry maintains a mapping of entities to their components, calls systems in order each tick, and supports creation and destruction of entities.

For a quick, visual overview of how systems interact with components and the execution order, see the ECS graph: docs/devlopper/ecs_graph.md

## Core components (data model)

- Transform: position in 2D space.
- Velocity: instantaneous velocity used by the movement integrator.
- Color: packed color value used for display.
- Network Type: category label for serialization into snapshots (player, enemy, bullet). This controls prioritization as well as which art to use on the client.
- Size: axis-aligned width/height used for simple AABB collision and on-screen culling.
- Player Input: input bitfield and movement speed to interpret desired movement and firing.
- Bullet Tag and Bullet Owner: identify bullets and their owning entity for scoring.
- Beam Tag: marks special beam-type bullets that can pass through multiple enemies in a single update.
- Enemy Tag: marks enemy entities for selection during collisions or AI.
- Enemy Shooter: parameters for enemies capable of firing (cooldown, interval, projectile speed, shot accuracy).
- Shooter (player): parameters for continuous fire while holding the shoot input (cooldown, interval, projectile speed).
- Charge Gun: charge-based shot state for players (accumulated time, cap, and trigger state).
- Score: player’s server-side score total.
- Hit Flag: marker set when a player is hit by an enemy projectile this tick.
- Invincible: temporary invulnerability window after being hit.
- Formation (origin) and Formation Follower: data needed to organize enemies into patterns (snake, line, grid, triangle) and move them coherently.

## System responsibilities (tick order)

The server initializes systems in a deterministic order so updates apply consistently each tick:

1. Input System: reads player input bits and directly integrates movement into player transforms based on configured speed. This is a simple server-side movement authority that clamps to speed only (boundary checks are handled elsewhere or on client visuals).
2. Shooting System (player): converts held shoot input into a sequence of small projectiles at fixed intervals, positioned relative to the player’s ship. Owner and size are set for scoring and collision.
3. Charge Shooting System (player): interprets the charge-shot input as press-and-hold, accumulating charge time. On release, it spawns a wide beam-like projectile whose thickness depends on charge duration. Beams can pass through multiple enemies in the same tick.
4. Formation System: updates formation origins and computes follower world positions. Snake formations add sinusoidal vertical motion; line, grid, and triangle arrange enemies into respective patterns. Followers inherit horizontal speed and their positions are clamped to the playable vertical band.
5. Movement System: integrates velocity for all entities, moving bullets, enemies, and formation origins.
6. Enemy Shooting System: for enemies with the shooter component, picks the nearest player and fires a projectile toward the target with configurable inaccuracy. Shot intervals and bullet speeds differ from players.
7. Despawn Offscreen System: removes entities that have moved past the left edge of the world.
8. Despawn Out Of Bounds System: removes bullets that leave the visible bounds in any direction. Players are not subject to this removal.
9. Collision System: performs simple AABB intersection between bullets and valid targets. Player bullets destroy enemies and grant score to the bullet’s owner. Enemy bullets hit players and set a hit marker, which the server processes after system updates.
10. Invincibility System: decrements invincibility timers every tick and clamps them to non-negative values.
11. Formation Spawn System: periodically creates new enemy formations (snake, line, grid, triangle, and larger shooter variants) within safe vertical margins. Spawn parameters vary (count, spacing, speed, accuracy for shooters).

After systems run, the server applies global rules:
- Player hit resolution: when a player has a hit marker, their lives are reduced (to a minimum of zero), they are respawned near the starting X position with a vertically clamped Y to keep them within the playable area, their velocity is reset, and an invincibility timer is added or extended. A lives update event is broadcast.
- Scoring aggregation: the team score is computed as the sum of all player scores and broadcast whenever it changes.

## Playable area and margins

- World height is standardized, and a top HUD margin and a bottom margin are reserved.
- Enemy followers are clamped vertically so formations do not overlap the HUD or leave the world.
- Player respawns select a safe Y coordinate within these margins.

## Serialization hints

- Net Type determines how entities are categorized for snapshot packing: player first priority, then bullets, then enemies. This influences how clients see the world if snapshots are truncated to fit within the UDP budget.

## Extensibility

- To add a new weapon or power-up: create components for state and a system to process them, then consider how they serialize (often piggybacking on existing packed-entity fields or adding a new event message).
- To add a new enemy pattern: introduce formation parameters and a new spawn helper, or a dedicated system if behavior is complex.
- To change difficulty: adjust spawn frequency, enemy speeds, accuracy, or bullet speed parameters.
- To support health or shields: consider augmenting the player data model and adding systems that tick down, regenerate, or absorb hits before reducing lives.
