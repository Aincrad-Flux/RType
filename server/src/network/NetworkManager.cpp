#include "network/NetworkManager.hpp"
#include <memory>
#include <iostream>

using namespace rtype::server::net;

NetworkManager::NetworkManager(unsigned short port)
    : socket_(io_, asio::ip::udp::endpoint(asio::ip::udp::v4(), port)) {}

NetworkManager::~NetworkManager() { stop(); }

void NetworkManager::setPacketHandler(PacketHandler cb) { onPacket_ = std::move(cb); }

void NetworkManager::start() {
    running_ = true;
    doReceive();
    ioThread_ = std::thread([this]{ io_.run(); });
    std::cout << "[server] Listening UDP on " << socket_.local_endpoint().port() << "\n";
}

void NetworkManager::stop() {
    running_ = false;
    if (!io_.stopped()) io_.stop();
    if (socket_.is_open()) { asio::error_code ec; socket_.close(ec); }
    if (ioThread_.joinable()) ioThread_.join();
}

void NetworkManager::doReceive() {
    socket_.async_receive_from(
        asio::buffer(buffer_), remote_,
        [this](std::error_code ec, std::size_t n) {
            if (!ec && onPacket_ && n >= sizeof(std::uint16_t)) // header size validated by handler
                onPacket_(remote_, buffer_.data(), n);
            if (!ec) doReceive();
        }
    );
}

void NetworkManager::send(const asio::ip::udp::endpoint& to, const void* data, std::size_t size) {
    auto buf = std::make_shared<std::vector<char>>(static_cast<const char*>(data), static_cast<const char*>(data) + size);
    socket_.async_send_to(asio::buffer(*buf), to, [buf](std::error_code, std::size_t) {});
}

void NetworkManager::broadcast(const void* data, std::size_t size) {
    for (const auto& [_, ep] : keyToEndpoint_) send(ep, data, size);
}

void NetworkManager::forEachEndpoint(const std::function<void(const asio::ip::udp::endpoint&)>& fn) const {
    for (const auto& [_, ep] : keyToEndpoint_) fn(ep);
}

std::string NetworkManager::makeKey(const asio::ip::udp::endpoint& ep) {
    return ep.address().to_string() + ":" + std::to_string(ep.port());
}

void NetworkManager::registerEndpoint(const std::string& key, const asio::ip::udp::endpoint& ep) {
    keyToEndpoint_[key] = ep;
    noteSeen(key);
}

void NetworkManager::unregisterEndpoint(const std::string& key) {
    keyToEndpoint_.erase(key);
    lastSeen_.erase(key);
}

void NetworkManager::noteSeen(const std::string& key) {
    lastSeen_[key] = std::chrono::steady_clock::now();
}

void NetworkManager::checkTimeouts(const std::function<void(const std::string& key)>& onTimeout) {
    using namespace std::chrono;
    const auto now = steady_clock::now();
    const auto timeout = seconds(10);
    std::vector<std::string> toRemove;
    for (auto& [key, last] : lastSeen_) {
        if (now - last > timeout) toRemove.push_back(key);
    }
    for (auto& key : toRemove) onTimeout(key);
}
