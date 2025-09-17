#pragma once
#include <vector>
#include "Types.hpp"

namespace rt::ecs {

class Registry;

class System {
  public:
    virtual ~System() = default;
    virtual void update(Registry& registry, float dt) = 0;
};

}
