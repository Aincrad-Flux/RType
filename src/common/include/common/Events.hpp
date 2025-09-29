#pragma once
#include "common/Protocol.hpp"
#include <vector>
#include <asio.hpp>

namespace rtype::common {

struct Event {
    net::Header header;
    std::vector<char> payload;
    asio::ip::udp::endpoint from;
};

}
