#pragma once
#include <asio.hpp>
#include <array>
#include <unordered_map>
#include <functional>
#include <thread>
#include <string>
#include <chrono>

namespace rtype::server::net {

class NetworkManager {
  public:
    explicit NetworkManager(unsigned short port);
    ~NetworkManager();

    using PacketHandler = std::function<void(const asio::ip::udp::endpoint&, const char*, std::size_t)>;

    void setPacketHandler(PacketHandler cb);
    void start();
    void stop();

    // sending helpers
    void send(const asio::ip::udp::endpoint& to, const void* data, std::size_t size);
    void broadcast(const void* data, std::size_t size);
    void forEachEndpoint(const std::function<void(const asio::ip::udp::endpoint&)>& fn) const;

    // endpoint bookkeeping
    static std::string makeKey(const asio::ip::udp::endpoint& ep);
    void registerEndpoint(const std::string& key, const asio::ip::udp::endpoint& ep);
    void unregisterEndpoint(const std::string& key);
    void noteSeen(const std::string& key);
    void checkTimeouts(const std::function<void(const std::string& key)>& onTimeout);

  private:
    void doReceive();

    asio::io_context io_;
    asio::ip::udp::socket socket_;
    std::array<char, 2048> buffer_{};
    asio::ip::udp::endpoint remote_;
    std::thread ioThread_;
    bool running_ = false;
    PacketHandler onPacket_;

    std::unordered_map<std::string, asio::ip::udp::endpoint> keyToEndpoint_;
    std::unordered_map<std::string, std::chrono::steady_clock::time_point> lastSeen_;
};

}
