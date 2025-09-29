#include "server/UdpServer.hpp"
#include "common/Protocol.hpp"
#include <iostream>
#include <csignal>
#include <atomic>
#include <thread>
#include <chrono>
#include <vector>
#include <arpa/inet.h> // for htons

static std::atomic<bool> g_running{true};
static rtype::server::UdpServer* g_server = nullptr;

void signal_handler(int) {
    std::cout << "\nStopping server...\n";
    g_running = false;
    if (g_server) g_server->stop();
}

void updateGame(/* your game world state object(s) */ int tickCount) {
    // TODO: replace with real ECS/system updates.
    // This stub represents processing physics, AI, spawning, etc.
    // For now we just log the tick.
    if ((tickCount % 60) == 0) {
        std::cout << "[GAME] Tick " << tickCount << "\n";
    }
}

int main(int argc, char** argv)
{
    unsigned short port = 4242;
    std::size_t threads = std::thread::hardware_concurrency();
    std::size_t ticks_per_second = 60;

    if (argc > 1) port = static_cast<unsigned short>(std::stoi(argv[1]));
    if (argc > 2) threads = static_cast<std::size_t>(std::stoi(argv[2]));
    if (argc > 3) ticks_per_second = static_cast<std::size_t>(std::stoi(argv[3]));

    std::cout << "Starting r-type_server on UDP port " << port
              << " with " << threads << " worker threads and " << ticks_per_second
              << " ticks/sec\n";

    rtype::server::UdpServer server(port, threads);
    g_server = &server;

    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    server.start();

    auto& queue = server.getEventQueue();

    // Launch game loop on its own thread (independent from io_context worker threads)
    std::thread gameLoopThread([&]() {
        using clock = std::chrono::steady_clock;
        const std::chrono::microseconds tickDuration(static_cast<int>(1'000'000 / ticks_per_second));
        auto nextTick = clock::now() + tickDuration;
        int tickCount = 0;

        while (g_running) {
            // 1) Consume all pending network events (non-blocking)
            for (;;) {
                auto maybeEvent = queue.try_pop();
                if (!maybeEvent) break;
                auto event = std::move(*maybeEvent);
                // handle event (dispatch to input buffers / player state)
                // For demo: print some info
                auto ip = event.from.address().to_string();
                auto portnum = event.from.port();
                if (event.header.type == rtype::net::MsgType::Input) {
                    std::cout << "[GAME] Received Input (" << event.header.size
                              << " bytes) from " << ip << ":" << portnum << "\n";
                    // TODO: enqueue input into player state
                } else if (event.header.type == rtype::net::MsgType::Hello) {
                    std::cout << "[GAME] Hello from " << ip << ":" << portnum << "\n";
                } else {
                    std::cout << "[GAME] MsgType " << static_cast<int>(event.header.type)
                              << " from " << ip << ":" << portnum << "\n";
                }
            }

            updateGame(tickCount);
            {
                rtype::net::Header header{};
                header.size = htons(static_cast<uint16_t>(sizeof(uint32_t))); // payload size
                header.type = rtype::net::MsgType::State;
                header.version = rtype::net::ProtocolVersion;

                uint32_t tick_be = htonl(static_cast<uint32_t>(tickCount)); // payload: tick number
                std::vector<char> packet;
                packet.resize(rtype::net::HeaderSize + sizeof(tick_be));
                std::memcpy(packet.data(), &header, rtype::net::HeaderSize);
                std::memcpy(packet.data() + rtype::net::HeaderSize, &tick_be, sizeof(tick_be));

                server.sendToAll(packet.data(), packet.size());
            }

            ++tickCount;

            std::this_thread::sleep_until(nextTick);
            nextTick += tickDuration;
        }

        std::cout << "[GAME] Game loop exiting.\n";
    });

    while (g_running) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    if (gameLoopThread.joinable()) gameLoopThread.join();

    std::cout << "Server stopped.\n";
    return 0;
}
