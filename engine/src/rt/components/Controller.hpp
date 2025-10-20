#pragma once
#include <cstdint>
namespace rt::components {
struct Controller {
    std::uint8_t bits = 0; // 0:Up 1:Down 2:Left 3:Right 4:Action
    float speed = 150.f;
};
}
