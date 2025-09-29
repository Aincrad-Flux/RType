#pragma once
#include <cstdint>
#include <array>

namespace rtype::net {

enum class MsgType : std::uint8_t {
    Hello = 1,
    HelloAck,
    Input,
    State,
    Spawn,
    Despawn,
    Ping,
    Pong
};

struct Header {
    std::uint16_t size;   // payload size excluding header
    MsgType type;
    std::uint8_t version;
};

static constexpr std::uint8_t ProtocolVersion = 1;
static constexpr std::size_t HeaderSize = sizeof(Header);

}
