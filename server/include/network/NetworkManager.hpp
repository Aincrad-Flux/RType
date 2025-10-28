#pragma once
#include <asio.hpp>
#include <memory>
#include "protocol/TcpServer.hpp"
#include "protocol/UdpServer.hpp"
#include "gameplay/GameSession.hpp"

namespace rtype::server::network {

class NetworkManager {
public:
    explicit NetworkManager(asio::io_context& io, unsigned short udpPort, unsigned short tcpPort);

    void start();
    void stop();

    rtype::server::TcpServer& tcp() { return *tcp_; }
    rtype::server::UdpServer& udp() { return *udp_; }

private:
    asio::io_context& io_;
    std::shared_ptr<rtype::server::TcpServer> tcp_;
    std::unique_ptr<rtype::server::UdpServer> udp_;
    std::unique_ptr<rtype::server::gameplay::GameSession> session_;
};

}
