#include "server/UdpServer.hpp"
#include <iostream>
#include <cstring>
#include <sstream>
#include <iomanip>
#include <vector>
#include <unordered_set>

namespace rtype::server {

UdpServer::UdpServer(unsigned short port)
    : socket_(io_, asio::ip::udp::endpoint(asio::ip::udp::v4(), port)) {}

UdpServer::~UdpServer() { stop(); }

void UdpServer::start() {
    try {
        auto ep = socket_.local_endpoint();
        std::cout << "[server] Listening on " << ep.address().to_string() << ":" << ep.port() << " (UDP)\n";

        // List host IPs
        try {
            asio::ip::udp::resolver res(io_);
            auto host = asio::ip::host_name();
            asio::ip::udp::resolver::results_type results = res.resolve(asio::ip::udp::v4(), host, "");
            std::cout << "[server] Hostname: " << host << "\n";
            std::cout << "[server] Addresses:" << "\n";
            std::unordered_set<std::string> seen;
            for (const auto &e : results) {
                auto addr = e.endpoint().address().to_string();
                if (seen.insert(addr).second) {
                    std::cout << "  - " << addr << "\n";
                }
            }
        } catch (...) {}
    } catch (const std::exception& e) {
        std::cout << "[server] start() info error: " << e.what() << "\n";
    }
    doReceive();
    thread_ = std::jthread([this]{ io_.run(); });
}

void UdpServer::stop() {
    if (!io_.stopped()) io_.stop();
    if (socket_.is_open()) {
        asio::error_code ec; socket_.close(ec);
    }
}

void UdpServer::doReceive() {
    socket_.async_receive_from(
        asio::buffer(buffer_), remote_,
        [this](std::error_code ec, std::size_t n) {
            if (n > 0) {
                // Trace incoming packet
                auto fromStr = remote_.address().to_string() + ":" + std::to_string(remote_.port());
                std::ostringstream oss;
                oss << std::hex << std::setfill('0');
                for (std::size_t i = 0; i < n && i < 64; ++i) {
                    oss << std::setw(2) << (static_cast<unsigned>(static_cast<unsigned char>(buffer_[i])))
                        << (i + 1 < n && i + 1 < 64 ? ' ' : '\0');
                }
                std::cout << "[recv] " << n << "B from " << fromStr << " | hex: " << oss.str() << "\n";
                if (!ec) {
                    handlePacket(remote_, buffer_.data(), n);
                }
            }
            if (ec) {
                std::cout << "[recv-error] code=" << ec.value() << " msg='" << ec.message() << "'\n";
            } else if (n == 0) {
                std::cout << "[recv] 0B datagram received\n";
            }
            if (!ec) doReceive();
        }
    );
}

void UdpServer::doSend(const asio::ip::udp::endpoint& to, const void* data, std::size_t size) {
    auto buf = std::make_shared<std::vector<char>>(static_cast<const char*>(data), static_cast<const char*>(data)+size);
    // Trace outgoing packet
    try {
        auto toStr = to.address().to_string() + ":" + std::to_string(to.port());
        std::ostringstream oss;
        oss << std::hex << std::setfill('0');
        for (std::size_t i = 0; i < buf->size() && i < 64; ++i) {
            oss << std::setw(2) << (static_cast<unsigned>(static_cast<unsigned char>((*buf)[i])))
                << (i + 1 < buf->size() && i + 1 < 64 ? ' ' : '\0');
        }
        std::cout << "[send] " << buf->size() << "B to " << toStr << " | hex: " << oss.str() << "\n";
    } catch (...) {}
    socket_.async_send_to(asio::buffer(*buf), to,
        [buf](std::error_code, std::size_t){});
}

void UdpServer::handlePacket(const asio::ip::udp::endpoint& from, const char* data, std::size_t size) {
    if (size < sizeof(rtype::net::Header)) return;
    const auto* header = reinterpret_cast<const rtype::net::Header*>(data);
    if (header->version != rtype::net::ProtocolVersion) return;
    const char* payload = data + sizeof(rtype::net::Header);
    std::size_t payloadSize = size - sizeof(rtype::net::Header);

    if (header->type == rtype::net::MsgType::Hello) {
        std::string username(payload, payload + payloadSize);
        std::string key = from.address().to_string() + ":" + std::to_string(from.port());
        endpointToUsername_[key] = username;

        // Build HelloAck + "OK"
        rtype::net::Header ackHdr{0, rtype::net::MsgType::HelloAck, rtype::net::ProtocolVersion};
        const char* ok = "OK";
        std::uint16_t okLen = 2;
        ackHdr.size = okLen;
        std::array<char, sizeof(rtype::net::Header) + 2> out{};
        std::memcpy(out.data(), &ackHdr, sizeof(ackHdr));
        std::memcpy(out.data() + sizeof(ackHdr), ok, okLen);
        doSend(from, out.data(), out.size());
        std::cout << "[hello] user='" << username << "' from " << key << "\n";
    }
}

}
