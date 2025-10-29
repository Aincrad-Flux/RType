#include "network/NetworkManager.hpp"
#include <iostream>

using namespace rtype::server::network;
using rtype::server::gameplay::GameSession;

NetworkManager::NetworkManager(asio::io_context& io, unsigned short udpPort, unsigned short tcpPort)
    : io_(io) {
    tcp_ = std::make_shared<rtype::server::TcpServer>(io_, tcpPort);
    tcp_->setUdpPort(udpPort);

    udp_ = std::make_unique<rtype::server::UdpServer>(io_, udpPort);
    udp_->setTcpServer(tcp_.get());

    session_ = std::make_unique<GameSession>(io_,
        [this](const asio::ip::udp::endpoint& to, const void* data, std::size_t size){
            udp_->sendRaw(to, data, size);
        },
        tcp_.get());

    // Bind TCP hello callback to session
    tcp_->setOnHello([this](const std::string& name, const std::string& ip){
        session_->onTcpHello(name, ip);
    });

    // Forward all UDP packets to session; first packet binds endpoint automatically
    udp_->setPacketHandler([this](const asio::ip::udp::endpoint& from, const char* data, std::size_t size){
        session_->onUdpPacket(from, data, size);
    });
}

void NetworkManager::start() {
    tcp_->start();
    udp_->start();
    session_->start();
}

void NetworkManager::stop() {
    session_->stop();
    udp_->stop();
    tcp_->stop();
}
