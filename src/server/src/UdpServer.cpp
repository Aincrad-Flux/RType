#include "server/UdpServer.hpp"
#include <iostream>

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

void UdpServer::start() {
    doReceive();
    for (std::size_t i = 0; i < workers_.capacity(); ++i) {
        workers_.emplace_back([this]{
            try {
                io_.run();
            } catch (const std::exception& e) {
                std::cerr << "Worker thread exception: " << e.what() << "\n";
            }
        });
    }
}


void UdpServer::stop() {
    workGuard_.reset();

    if (!io_.stopped())
        io_.stop();

    if (socket_.is_open()) {
        asio::error_code ec;
        socket_.close(ec);
    }

    workers_.clear(); // joins all jthreads
}

void UdpServer::doReceive() {
    socket_.async_receive_from(
        asio::buffer(buffer_), remote_,
        [this](std::error_code ec, std::size_t n) {
            if (!ec && n > 0) {
                doSend(remote_, buffer_.data(), n); // echo
            } else if (ec) {
                std::cerr << "Receive error: " << ec.message() << "\n";
            }
            doReceive();
        }
    );
}

void UdpServer::doSend(const asio::ip::udp::endpoint& to, const void* data, std::size_t size) {
    auto buf = std::make_shared<std::vector<char>>(
        static_cast<const char*>(data),
        static_cast<const char*>(data) + size
    );

    socket_.async_send_to(asio::buffer(*buf), to,
        [buf](std::error_code, std::size_t) {
        }
    );
}

}
