#pragma once
#include <asio.hpp>
#include <thread>
#include <array>
#include <vector>
#include <memory>
#include <mutex>
#include <algorithm>
#include "common/Event.hpp"
#include "common/EventQueue.hpp"

namespace rtype::server {

class UdpServer {
  public:
    explicit UdpServer(unsigned short port, std::size_t threadCount = std::thread::hardware_concurrency());
    ~UdpServer();

    void start();
    void stop();

    rtype::common::EventQueue<rtype::common::Event>& getEventQueue() { return eventQueue_; }
    void sendToAll(const void* data, std::size_t size);

  private:
    void doReceive();
    void doSend(const asio::ip::udp::endpoint& to, const void* data, std::size_t size);

    asio::io_context io_;
    asio::executor_work_guard<asio::io_context::executor_type> workGuard_;
    asio::ip::udp::socket socket_;
    std::array<char, 2048> buffer_{};
    asio::ip::udp::endpoint remote_;
    std::vector<std::jthread> workers_;
    std::size_t threadCount_;
    rtype::common::EventQueue<rtype::common::Event> eventQueue_;

    std::vector<asio::ip::udp::endpoint> clients_;
    std::mutex clientsMutex_;
};

}
