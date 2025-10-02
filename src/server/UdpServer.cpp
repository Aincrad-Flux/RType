#include "UdpServer.hpp"
#include <iostream>
#include <cstring>

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
            // Create a player entity
            ServerEntity e{};
            e.id = nextEntityId_++;
            e.type = rtype::net::EntityType::Player;
            e.x = 50.f;
            e.y = 100.f + static_cast<float>(endpointToPlayerId_.size()) * 40.f;
            e.vx = 0.f; e.vy = 0.f;
            e.rgba = 0x55AAFFFFu; // cyan-ish
            entities_.push_back(e);
            endpointToPlayerId_[key] = e.id;
            playerInputBits_[e.id] = 0;
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
    double enemySpawnTimer = 0.0;

    while (running_) {
        next += std::chrono::duration_cast<clock::duration>(std::chrono::duration<double>(dt));

        // Apply inputs to players
        for (auto& ent : entities_) {
            if (ent.type != rtype::net::EntityType::Player) continue;
            std::uint8_t bits = playerInputBits_[ent.id];
            float speed = 150.f;
            float vx = 0.f, vy = 0.f;
            if (bits & rtype::net::InputLeft)  vx -= speed;
            if (bits & rtype::net::InputRight) vx += speed;
            if (bits & rtype::net::InputUp)    vy -= speed;
            if (bits & rtype::net::InputDown)  vy += speed;
            ent.x += vx * static_cast<float>(dt);
            ent.y += vy * static_cast<float>(dt);
        }

        // Simple enemy spawn and movement
        enemySpawnTimer += dt;
        if (enemySpawnTimer > 2.0) {
            enemySpawnTimer = 0.0;
            std::uniform_real_distribution<float> ydist(20.f, 500.f);
            ServerEntity e{};
            e.id = nextEntityId_++;
            e.type = rtype::net::EntityType::Enemy;
            e.x = 900.f; e.y = ydist(rng_);
            e.vx = -60.f; e.vy = 0.f;
            e.rgba = 0xFF5555FFu;
            entities_.push_back(e);
        }
        for (auto& ent : entities_) {
            if (ent.type == rtype::net::EntityType::Enemy) {
                ent.x += ent.vx * static_cast<float>(dt);
            }
        }
        // Remove off-screen enemies
        entities_.erase(std::remove_if(entities_.begin(), entities_.end(), [](const ServerEntity& e){
            return e.type == rtype::net::EntityType::Enemy && e.x < -50.f;
        }), entities_.end());

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
    sh.count = static_cast<std::uint16_t>(std::min<size_t>(entities_.size(), 512));

    std::size_t payloadSize = sizeof(rtype::net::StateHeader) + sh.count * sizeof(rtype::net::PackedEntity);
    hdr.size = static_cast<std::uint16_t>(payloadSize);

    std::vector<char> out(sizeof(rtype::net::Header) + payloadSize);
    std::memcpy(out.data(), &hdr, sizeof(hdr));
    std::memcpy(out.data() + sizeof(hdr), &sh, sizeof(sh));
    auto* arr = reinterpret_cast<rtype::net::PackedEntity*>(out.data() + sizeof(hdr) + sizeof(sh));
    for (std::uint16_t i = 0; i < sh.count; ++i) {
        const auto& e = entities_[i];
        arr[i].id = e.id;
        arr[i].type = e.type;
        arr[i].x = e.x; arr[i].y = e.y;
        arr[i].vx = e.vx; arr[i].vy = e.vy;
        arr[i].rgba = e.rgba;
    }

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
