#include "UdpServer.hpp"
#include <iostream>
#include <cstring>
#include <cmath>
#include "rt/game/Components.hpp"
#include "rt/game/Systems.hpp"

namespace rtype::server {

static std::string makeKey(const asio::ip::udp::endpoint& ep) {
    return ep.address().to_string() + ":" + std::to_string(ep.port());
}

UdpServer::UdpServer(unsigned short port)
    : socket_(io_, asio::ip::udp::endpoint(asio::ip::udp::v4(), port)), rng_(std::random_device{}()) {}

UdpServer::~UdpServer() {
    stop();
}

void UdpServer::start() {
    std::cout << "[server] Listening UDP on " << socket_.local_endpoint().port() << "\n";
    doReceive();
    running_ = true;
    netThread_ = std::thread([this]{ io_.run(); });
    gameThread_ = std::thread([this]{ gameLoop(); });
}

void UdpServer::stop() {
    running_ = false;
    if (!io_.stopped()) io_.stop();
    if (socket_.is_open()) {
        asio::error_code ec; socket_.close(ec);
    }
    if (netThread_.joinable()) netThread_.join();
    if (gameThread_.joinable()) gameThread_.join();
}

void UdpServer::doReceive() {
    socket_.async_receive_from(
        asio::buffer(buffer_), remote_,
        [this](std::error_code ec, std::size_t n) {
            if (!ec && n >= sizeof(rtype::net::Header)) {
                handlePacket(remote_, buffer_.data(), n);
            }
            if (!ec) doReceive();
        }
    );
}

void UdpServer::handlePacket(const asio::ip::udp::endpoint& from, const char* data, std::size_t size) {
    if (size < sizeof(rtype::net::Header)) return;
    const auto* header = reinterpret_cast<const rtype::net::Header*>(data);
    if (header->version != rtype::net::ProtocolVersion) return;
    const char* payload = data + sizeof(rtype::net::Header);
    std::size_t payloadSize = size - sizeof(rtype::net::Header);

    auto key = makeKey(from);
    if (header->type == rtype::net::MsgType::Hello) {
        keyToEndpoint_[key] = from;
        if (endpointToPlayerId_.find(key) == endpointToPlayerId_.end()) {
            // Create a player entity in ECS
            auto e = reg_.create();
            reg_.emplace<rt::game::Transform>(e, {50.f, 100.f + static_cast<float>(endpointToPlayerId_.size()) * 40.f});
            reg_.emplace<rt::game::Velocity>(e, {0.f, 0.f});
            reg_.emplace<rt::game::NetType>(e, {rtype::net::EntityType::Player});
            reg_.emplace<rt::game::ColorRGBA>(e, {0x55AAFFFFu});
            reg_.emplace<rt::game::PlayerInput>(e, {0, 150.f});
            reg_.emplace<rt::game::Shooter>(e, {0.f, 0.15f, 320.f});
            reg_.emplace<rt::game::Size>(e, {20.f, 12.f});
            endpointToPlayerId_[key] = e;
            playerInputBits_[e] = 0;
        }
        // Reply HelloAck
        rtype::net::Header ack{0, rtype::net::MsgType::HelloAck, rtype::net::ProtocolVersion};
        doSend(from, &ack, sizeof(ack));
        return;
    }

    if (header->type == rtype::net::MsgType::Input) {
        if (payloadSize >= sizeof(rtype::net::InputPacket)) {
            auto* in = reinterpret_cast<const rtype::net::InputPacket*>(payload);
            auto it = endpointToPlayerId_.find(key);
            if (it != endpointToPlayerId_.end()) {
                playerInputBits_[it->second] = in->bits;
                if (auto* pi = reg_.get<rt::game::PlayerInput>(it->second)) pi->bits = in->bits;
            }
        }
        return;
    }
}

void UdpServer::gameLoop() {
    using clock = std::chrono::steady_clock;
    const double tickRate = 60.0;
    const double dt = 1.0 / tickRate;
    auto next = clock::now();
    float elapsed = 0.f;
    // Install systems once
    reg_.addSystem(std::make_unique<rt::game::InputSystem>());
    reg_.addSystem(std::make_unique<rt::game::ShootingSystem>());
    reg_.addSystem(std::make_unique<rt::game::FormationSystem>(&elapsed));
    reg_.addSystem(std::make_unique<rt::game::MovementSystem>());
    reg_.addSystem(std::make_unique<rt::game::EnemyShootingSystem>(rng_));
    reg_.addSystem(std::make_unique<rt::game::DespawnOffscreenSystem>(-50.f));
    // Despawn bullets that leave screen (x: -50..1000, y: -50..600)
    reg_.addSystem(std::make_unique<rt::game::DespawnOutOfBoundsSystem>(-50.f, 1000.f, -50.f, 600.f));
    reg_.addSystem(std::make_unique<rt::game::CollisionSystem>());
    reg_.addSystem(std::make_unique<rt::game::FormationSpawnSystem>(rng_, &elapsed));

    while (running_) {
        next += std::chrono::duration_cast<clock::duration>(std::chrono::duration<double>(dt));
    elapsed += static_cast<float>(dt);

        // Tick ECS
        reg_.update(static_cast<float>(dt));

        broadcastState();

        std::this_thread::sleep_until(next);
    }
}


void UdpServer::broadcastState() {
    // Build State packet
    rtype::net::Header hdr{};
    hdr.version = rtype::net::ProtocolVersion;
    hdr.type = rtype::net::MsgType::State;
    rtype::net::StateHeader sh{};
    // collect ECS entities with NetType for serialization
    std::vector<rtype::net::PackedEntity> net;
    net.reserve(512);
    auto& types = reg_.storage<rt::game::NetType>().data();
    for (auto& [e, nt] : types) {
        auto* tr = reg_.get<rt::game::Transform>(e);
        auto* ve = reg_.get<rt::game::Velocity>(e);
        auto* co = reg_.get<rt::game::ColorRGBA>(e);
        if (!tr || !ve || !co) continue;
        rtype::net::PackedEntity pe{};
        pe.id = e;
        pe.type = nt.type;
        pe.x = tr->x; pe.y = tr->y;
        pe.vx = ve->vx; pe.vy = ve->vy;
        pe.rgba = co->rgba;
        net.push_back(pe);
        if (net.size() >= 512) break;
    }
    sh.count = static_cast<std::uint16_t>(net.size());

    std::size_t payloadSize = sizeof(rtype::net::StateHeader) + net.size() * sizeof(rtype::net::PackedEntity);
    hdr.size = static_cast<std::uint16_t>(payloadSize);

    std::vector<char> out(sizeof(rtype::net::Header) + payloadSize);
    std::memcpy(out.data(), &hdr, sizeof(hdr));
    std::memcpy(out.data() + sizeof(hdr), &sh, sizeof(sh));
    auto* arr = reinterpret_cast<rtype::net::PackedEntity*>(out.data() + sizeof(hdr) + sizeof(sh));
    for (std::uint16_t i = 0; i < sh.count; ++i) arr[i] = net[i];

    for (const auto& [key, ep] : keyToEndpoint_) {
        doSend(ep, out.data(), out.size());
    }
}

void UdpServer::doSend(const asio::ip::udp::endpoint& to, const void* data, std::size_t size) {
    auto buf = std::make_shared<std::vector<char>>(static_cast<const char*>(data), static_cast<const char*>(data)+size);
    socket_.async_send_to(asio::buffer(*buf), to,
        [buf](std::error_code, std::size_t){});
}

}
