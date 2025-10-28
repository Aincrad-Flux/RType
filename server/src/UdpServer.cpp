#include "UdpServer.hpp"
#include "network/NetworkManager.hpp"
#include "instance/MatchInstance.hpp"

using namespace rtype::server;

UdpServer::UdpServer(unsigned short port)
    : net_(new net::NetworkManager(port)), inst_(nullptr) {
    inst_ = new instance::MatchInstance(*net_);
}

UdpServer::~UdpServer() { stop(); delete inst_; delete net_; }

void UdpServer::start() {
    // Wire packet handling to instance
    net_->setPacketHandler([this](const asio::ip::udp::endpoint& from, const char* data, std::size_t size){
        this->inst_->handlePacket(from, data, size);
    });
    net_->start();
    inst_->start();
}

void UdpServer::stop() {
    if (!net_ && !inst_) return;
    if (inst_) inst_->stop();
    if (net_) net_->stop();
}

void UdpServer::maybeStartGame() {
    // Option C: configurable later; use 2 players for now
    const std::size_t required = 2;
    if (!gameStarted_ && endpointToPlayerId_.size() >= required) {
        gameStarted_ = true;
        if (tcp_) {
            std::cout << "[server] Enough players joined (" << endpointToPlayerId_.size()
                      << "). Sending StartGame over TCP.\n";
            tcp_->broadcastStartGame();
        }
    }
}
