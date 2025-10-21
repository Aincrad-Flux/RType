#pragma once
#include <cstdint>

namespace rt::ecs {
using Entity = std::uint32_t;
static constexpr Entity kInvalidEntity = 0;

// Small, typed bits for input or flags when helpful.
using Bits8 = std::uint8_t;
}
