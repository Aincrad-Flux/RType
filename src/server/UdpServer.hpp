#pragma once
#include <asio.hpp>
#include <thread>
#include <array>

namespace rtype::server {

class UdpServer {
  public:
    explicit UdpServer(unsigned short port);
    ~UdpServer();
    void start();
    void stop();
  private:
    void doReceive();
    void doSend(const asio::ip::udp::endpoint& to, const void* data, std::size_t size);

    asio::io_context io_;
    asio::ip::udp::socket socket_;
    std::array<char, 2048> buffer_{};
    asio::ip::udp::endpoint remote_;
    std::jthread thread_;
};

}
