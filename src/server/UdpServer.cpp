#include "UdpServer.hpp"
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
            if (!ec && n > 0) {
                doSend(remote_, buffer_.data(), n); // echo
            }
            if (!ec) doReceive();
        }
    );
}

void UdpServer::doSend(const asio::ip::udp::endpoint& to, const void* data, std::size_t size) {
    auto buf = std::make_shared<std::vector<char>>(static_cast<const char*>(data), static_cast<const char*>(data)+size);
    socket_.async_send_to(asio::buffer(*buf), to,
        [buf](std::error_code, std::size_t){});
}

}
