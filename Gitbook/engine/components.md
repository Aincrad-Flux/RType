---
icon: package
---

# Components

A quick reference for common components used by systems.

- Transform: position (x, y)
- Velocity: velocity (vx, vy)
- ColorRGBA: packed 0xRRGGBBAA
- NetType: category for snapshot packing (player, enemy, bullet)
- Size: width/height for AABB checks
- PlayerInput: input bits and movement speed
- Shooter: player shooting configuration (cooldown, interval, bullet speed)
- ChargeGun: charge-shot state (accumulated time, limits)
- EnemyTag: marks enemies
- EnemyShooter: enemy fire configuration (interval, bullet speed, accuracy)
- BulletTag: marks bullets
- BulletOwner: entity id of the shooter (for scoring)
- BeamTag: marks beam-type bullets
- Score: per-player score tally
- HitFlag: mark set when a player is hit this tick
- Invincible: remaining invincibility time
- Formation / FormationFollower: parameters for formation origins and followers

Notes:
- Prefer small POD structs; avoid heavy constructors.
- Keep serialization impact in mind when adding fields.
