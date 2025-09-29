#include "UdpServer.hpp"
#include "../../common/Protocol.hpp"
#include <iostream>

namespace rtype::server {

UdpServer::UdpServer(unsigned short port)
    : socket_(io_, asio::ip::udp::endpoint(asio::ip::udp::v4(), port)) {}

UdpServer::~UdpServer() {
    stop();
    if (thread_.joinable()) thread_.join();
}

void UdpServer::start() {
    doReceive();
    thread_ = std::thread([this]{ io_.run(); });
}

void UdpServer::stop() {
    if (!io_.stopped()) io_.stop();
    if (thread_.joinable()) thread_.join();
    if (socket_.is_open()) {
        asio::error_code ec; socket_.close(ec);
    }
}

void UdpServer::doReceive() {
    socket_.async_receive_from(
        asio::buffer(buffer_), remote_,
        [this](std::error_code ec, std::size_t n) {
            if (!ec && n >= sizeof(rtype::net::Header)) {
                // Log reception
                std::cout << "[RECV] From " << remote_.address().to_string() << ":" << remote_.port() << " (" << n << " bytes): ";
                for (size_t i = 0; i < n; ++i) {
                    printf("%02x", static_cast<unsigned char>(buffer_[i]));
                }
                std::cout << std::endl;

                // Analyse du header
                auto* header = reinterpret_cast<rtype::net::Header*>(buffer_.data());
                if (header->type == rtype::net::MsgType::Hello) {
                    // Préparer HelloAck
                    rtype::net::Header ackHeader;
                    ackHeader.size = 0;
                    ackHeader.type = rtype::net::MsgType::HelloAck;
                    ackHeader.version = rtype::net::ProtocolVersion;
                    doSend(remote_, &ackHeader, sizeof(ackHeader));
                } else {
                    doSend(remote_, buffer_.data(), n); // echo pour les autres paquets
                }
            }
            if (!ec) doReceive();
        }
    );
}

void UdpServer::doSend(const asio::ip::udp::endpoint& to, const void* data, std::size_t size) {
    auto buf = std::make_shared<std::vector<char>>(static_cast<const char*>(data), static_cast<const char*>(data)+size);
    // Log envoi
    std::cout << "[SEND] To " << to.address().to_string() << ":" << to.port() << " (" << size << " bytes): ";
    for (size_t i = 0; i < size; ++i) {
        printf("%02x", static_cast<unsigned char>((*buf)[i]));
    }
    std::cout << std::endl;
    socket_.async_send_to(asio::buffer(*buf), to,
        [buf](std::error_code, std::size_t){});
}

}
