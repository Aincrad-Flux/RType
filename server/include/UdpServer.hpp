#pragma once

namespace rtype::server {

// Forward declare new internal modules
namespace net { class NetworkManager; }
namespace instance { class MatchInstance; }

// Facade that wires the networking and gameplay/instance modules together
class UdpServer {
public:
    UdpServer(asio::io_context& io, unsigned short port);
    ~UdpServer();

    void start();
    void stop();
  private:
    net::NetworkManager* net_;
    instance::MatchInstance* inst_;
};

}
