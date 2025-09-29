#include "server/UdpServer.hpp"
#include <iostream>
#include <cstring>
#include <arpa/inet.h>

namespace rtype::server {

UdpServer::UdpServer(unsigned short port, std::size_t threadCount)
    : workGuard_(asio::make_work_guard(io_)),
      socket_(io_, asio::ip::udp::endpoint(asio::ip::udp::v4(), port)),
      threadCount_(threadCount)
{
    workers_.reserve(threadCount_);
}

UdpServer::~UdpServer() {
    stop();
}

void UdpServer::start()
{
    doReceive();
    for (std::size_t i = 0; i < threadCount_; ++i) {
        workers_.emplace_back([this]{
            try {
                io_.run();
            } catch (const std::exception& e) {
                std::cerr << "Worker thread exception: " << e.what() << "\n";
            }
        });
    }
}

void UdpServer::stop()
{
    workGuard_.reset();

    if (!io_.stopped())
        io_.stop();

    if (socket_.is_open()) {
        asio::error_code ec;
        socket_.close(ec);
        if (ec) std::cerr << "[WARN] socket close error: " << ec.message() << "\n";
    }

    workers_.clear(); // joins all jthreads
}

void UdpServer::doReceive()
{
    socket_.async_receive_from(
        asio::buffer(buffer_), remote_,
        [this](std::error_code ec, std::size_t n) {
            if (ec) {
                if (ec == asio::error::operation_aborted || ec == asio::error::bad_descriptor) {
                    return;
                }
                std::cerr << "[ERROR] receive: " << ec.message() << "\n";
                doReceive();
                return;
            }

            // Register client if not present (thread-safe)
            {
                std::scoped_lock lock(clientsMutex_);
                auto it = std::find_if(clients_.begin(), clients_.end(),
                    [this](const asio::ip::udp::endpoint& e) {
                        return e.address() == remote_.address() && e.port() == remote_.port();
                    });
                if (it == clients_.end()) {
                    clients_.push_back(remote_);
                    std::cout << "[NET] New client: " << remote_.address().to_string()
                              << ":" << remote_.port() << "\n";
                }
            }

            if (n >= rtype::net::HeaderSize) {
                rtype::net::Header header;
                std::memcpy(&header, buffer_.data(), sizeof(header));

                std::uint16_t payload_size = ntohs(header.size);
                if (payload_size > buffer_.size() - rtype::net::HeaderSize) {
                    std::cerr << "[ERROR] payload_size too large: " << payload_size << "\n";
                } else if (n >= rtype::net::HeaderSize + payload_size) {
                    if (header.version == rtype::net::ProtocolVersion) {
                        std::vector<char> payload;
                        payload.reserve(payload_size);
                        payload.insert(payload.end(),
                                       buffer_.begin() + rtype::net::HeaderSize,
                                       buffer_.begin() + rtype::net::HeaderSize + payload_size);

                        header.size = payload_size;

                        rtype::common::Event ev{header, std::move(payload), remote_};
                        eventQueue_.push(std::move(ev));
                    } else {
                        std::cerr << "[ERROR] Protocol version mismatch (got "
                                  << static_cast<int>(header.version) << " expected "
                                  << static_cast<int>(rtype::net::ProtocolVersion) << ")\n";
                    }
                } else {
                    std::cerr << "[ERROR] Received length (" << n << ") smaller than header+payload ("
                              << (rtype::net::HeaderSize + payload_size) << ")\n";
                }
            } else {
                std::cerr << "[WARN] Received too small packet (" << n << " bytes)\n";
            }
            doReceive();
        }
    );
}

void UdpServer::doSend(const asio::ip::udp::endpoint& to, const void* data, std::size_t size)
{
    auto buf = std::make_shared<std::vector<char>>(
        static_cast<const char*>(data),
        static_cast<const char*>(data) + size
    );

    socket_.async_send_to(asio::buffer(*buf), to,
        [buf](std::error_code ec, std::size_t bytes_sent) {
            if (ec) {
                std::cerr << "[ERROR] async_send_to: " << ec.message() << "\n";
            }
        }
    );
}

void UdpServer::sendToAll(const void* data, std::size_t size)
{
    std::scoped_lock lock(clientsMutex_);
    for (const auto& client : clients_) {
        doSend(client, data, size);
    }
}

}