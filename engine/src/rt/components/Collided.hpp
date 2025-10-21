#pragma once
namespace rt::components {
// Marker added by CollisionSystem when an entity collides with something interesting (e.g., player vs enemy)
struct Collided { bool value = true; };
}
