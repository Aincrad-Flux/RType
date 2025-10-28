#pragma once
#include <asio.hpp>
#include <array>
#include <functional>
#include <memory>

namespace rtype::server {

class TcpServer;

class UdpServer {
public:
    using PacketHandler = std::function<void(const asio::ip::udp::endpoint&, const char*, std::size_t)>;

    UdpServer(asio::io_context& io, unsigned short port);
    ~UdpServer();
    void start();
    void stop();
    void setPacketHandler(PacketHandler handler) { handler_ = std::move(handler); }
    void sendRaw(const asio::ip::udp::endpoint& to, const void* data, std::size_t size);
    void setTcpServer(TcpServer* tcp) { tcp_ = tcp; }

private:
    void doReceive();

private:
    asio::io_context& io_;
    asio::ip::udp::socket socket_;
    std::array<char, 2048> buffer_{};
    asio::ip::udp::endpoint remote_;
    bool running_ = false;

    PacketHandler handler_{};
    TcpServer* tcp_ = nullptr; // to be removed later
};

}

