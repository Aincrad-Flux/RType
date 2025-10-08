#include "UdpServer.hpp"
#include <iostream>
#include <cstring>
#include <cmath>
#include <chrono>
#include "rt/game/Components.hpp"
#include "rt/game/Systems.hpp"

namespace rtype::server {

static std::string makeKey(const asio::ip::udp::endpoint& ep) {
    return ep.address().to_string() + ":" + std::to_string(ep.port());
}

UdpServer::UdpServer(unsigned short port)
    : socket_(io_, asio::ip::udp::endpoint(asio::ip::udp::v4(), port)), rng_(std::random_device{}()) {}

UdpServer::~UdpServer() { stop(); }

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
            if (!ec && n >= sizeof(rtype::net::Header))
                handlePacket(remote_, buffer_.data(), n);
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
    lastSeen_[key] = std::chrono::steady_clock::now();

    if (header->type == rtype::net::MsgType::Hello) {
        keyToEndpoint_[key] = from;
        if (endpointToPlayerId_.find(key) == endpointToPlayerId_.end()) {
            auto e = reg_.create();
            reg_.emplace<rt::game::Transform>(e, {50.f, 100.f + static_cast<float>(endpointToPlayerId_.size()) * 40.f});
            reg_.emplace<rt::game::Velocity>(e, {0.f, 0.f});
            reg_.emplace<rt::game::NetType>(e, {rtype::net::EntityType::Player});
            reg_.emplace<rt::game::ColorRGBA>(e, {0x55AAFFFFu});
            reg_.emplace<rt::game::PlayerInput>(e, {0, 150.f});
            reg_.emplace<rt::game::Shooter>(e, {0.f, 0.15f, 320.f});
            reg_.emplace<rt::game::Size>(e, {20.f, 12.f});
            reg_.emplace<rt::game::Score>(e, {0});
            endpointToPlayerId_[key] = e;
            playerInputBits_[e] = 0;
            // Default lives
            playerLives_[e] = 4;
            playerScores_[e] = 0;
            // Parse optional username in payload
            std::string uname;
            if (payloadSize > 0) {
                uname.assign(payload, payload + std::min<std::size_t>(payloadSize, 15));
                // strip trailing nulls/spaces
                while (!uname.empty() && (uname.back() == '\0' || uname.back() == ' ')) uname.pop_back();
            }
            if (uname.empty()) uname = "Player" + std::to_string(e);
            playerNames_[e] = uname;
            // Send roster immediately on new join
            broadcastRoster();
        }
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
                if (auto* pi = reg_.get<rt::game::PlayerInput>(it->second))
                    pi->bits = in->bits;
            }
        }
        return;
    }

    if (header->type == rtype::net::MsgType::Disconnect) {
        // Client explicitly disconnects
        removeClient(key);
        return;
    }
}

void UdpServer::gameLoop() {
    using clock = std::chrono::steady_clock;
    const double tickRate = 60.0;
    const double dt = 1.0 / tickRate;
    auto next = clock::now();
    float elapsed = 0.f;
    reg_.addSystem(std::make_unique<rt::game::InputSystem>());
    reg_.addSystem(std::make_unique<rt::game::ShootingSystem>());
    reg_.addSystem(std::make_unique<rt::game::FormationSystem>(&elapsed));
    reg_.addSystem(std::make_unique<rt::game::MovementSystem>());
    reg_.addSystem(std::make_unique<rt::game::EnemyShootingSystem>(rng_));
    reg_.addSystem(std::make_unique<rt::game::DespawnOffscreenSystem>(-50.f));
    reg_.addSystem(std::make_unique<rt::game::DespawnOutOfBoundsSystem>(-50.f, 1000.f, -50.f, 600.f));
    reg_.addSystem(std::make_unique<rt::game::CollisionSystem>());
    reg_.addSystem(std::make_unique<rt::game::InvincibilitySystem>());
    reg_.addSystem(std::make_unique<rt::game::FormationSpawnSystem>(rng_, &elapsed));

    while (running_) {
        next += std::chrono::duration_cast<clock::duration>(std::chrono::duration<double>(dt));
        elapsed += static_cast<float>(dt);
        reg_.update(static_cast<float>(dt));
        // After systems, handle player hits -> decrement lives and respawn
        for (auto& [e, inp] : reg_.storage<rt::game::PlayerInput>().data()) {
            (void)inp;
            if (auto* hf = reg_.get<rt::game::HitFlag>(e)) {
                if (hf->value) {
                    auto lives = playerLives_[e];
                    if (lives > 0) {
                        lives = static_cast<std::uint8_t>(lives - 1);
                        playerLives_[e] = lives;
                        broadcastLivesUpdate(e, lives);
                    }
                    // Respawn player at starting X with preserved Y within bounds
                    if (auto* t = reg_.get<rt::game::Transform>(e)) {
                        constexpr float kStartX = 50.f;
                        constexpr float kWorldH = 600.f;
                        constexpr float kTopMargin = 56.f;
                        constexpr float kBottomMargin = 10.f;
                        float y = t->y;
                        // Clamp Y to safe region
                        float maxY = kWorldH - kBottomMargin - 12.f; // approx player height
                        if (y < kTopMargin) y = kTopMargin;
                        if (y > maxY) y = maxY;
                        t->x = kStartX; t->y = y;
                    }
                    // Reset velocity
                    if (auto* v = reg_.get<rt::game::Velocity>(e)) { v->vx = 0.f; v->vy = 0.f; }
                    // Give a short invincibility if not already
                    if (auto* inv = reg_.get<rt::game::Invincible>(e)) {
                        inv->timeLeft = std::max(inv->timeLeft, 1.0f);
                    } else {
                        reg_.emplace<rt::game::Invincible>(e, {1.0f});
                    }
                    // Clear hit flag for next tick
                    hf->value = false;
                }
            }
        }

        // After systems, sync score deltas from ECS and broadcast
        for (auto& [e, inp] : reg_.storage<rt::game::PlayerInput>().data()) {
            (void)inp;
            if (auto* sc = reg_.get<rt::game::Score>(e)) {
                auto prev = playerScores_[e];
                if (sc->value != prev) {
                    playerScores_[e] = sc->value;
                    // broadcast score update
                    rtype::net::Header hdr{};
                    hdr.version = rtype::net::ProtocolVersion;
                    hdr.type = rtype::net::MsgType::ScoreUpdate;
                    hdr.size = sizeof(rtype::net::ScoreUpdatePayload);
                    rtype::net::ScoreUpdatePayload p{ e, sc->value };
                    std::vector<char> out(sizeof(hdr) + sizeof(p));
                    std::memcpy(out.data(), &hdr, sizeof(hdr));
                    std::memcpy(out.data() + sizeof(hdr), &p, sizeof(p));
                    for (const auto& [_, ep] : keyToEndpoint_) doSend(ep, out.data(), out.size());
                }
            }
        }
        checkTimeouts();
        broadcastState();
        std::this_thread::sleep_until(next);
    }
}

void UdpServer::checkTimeouts() {
    using namespace std::chrono;
    const auto now = steady_clock::now();
    const auto timeout = seconds(10);
    std::vector<std::string> toRemove;
    for (auto& [key, last] : lastSeen_) {
        if (now - last > timeout)
            toRemove.push_back(key);
    }
    for (auto& key : toRemove)
        removeClient(key);
}

void UdpServer::removeClient(const std::string& key) {
    auto it = endpointToPlayerId_.find(key);
    if (it == endpointToPlayerId_.end()) return;
    auto id = it->second;
    endpointToPlayerId_.erase(it);
    keyToEndpoint_.erase(key);
    lastSeen_.erase(key);
    playerInputBits_.erase(id);
    try { reg_.destroy(id); } catch (...) {}
    rtype::net::Header hdr{};
    hdr.size = sizeof(std::uint32_t);
    hdr.type = rtype::net::MsgType::Despawn;
    hdr.version = rtype::net::ProtocolVersion;
    std::vector<char> out(sizeof(hdr) + sizeof(std::uint32_t));
    std::memcpy(out.data(), &hdr, sizeof(hdr));
    std::memcpy(out.data() + sizeof(hdr), &id, sizeof(id));
    for (auto& [_, ep] : keyToEndpoint_)
        doSend(ep, out.data(), out.size());
    // Send roster once to update remaining clients' top bar
    broadcastRoster();
    std::cout << "[server] Removed disconnected client: " << key << "\n";

    // If less than 2 players remain, notify remaining players to return to menu
    if (endpointToPlayerId_.size() > 0 && endpointToPlayerId_.size() < 2) {
        rtype::net::Header rtm{0, rtype::net::MsgType::ReturnToMenu, rtype::net::ProtocolVersion};
        for (auto& [_, ep] : keyToEndpoint_) {
            doSend(ep, &rtm, sizeof(rtm));
        }
    }
}

void UdpServer::broadcastState() {
    rtype::net::Header hdr{};
    hdr.version = rtype::net::ProtocolVersion;
    hdr.type = rtype::net::MsgType::State;
    rtype::net::StateHeader sh{};
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
    for (const auto& [key, ep] : keyToEndpoint_)
        doSend(ep, out.data(), out.size());
}

void UdpServer::broadcastRoster() {
    // Build roster entries from current endpointToPlayerId_
    rtype::net::RosterHeader rh{};
    std::vector<rtype::net::PlayerEntry> entries;
    entries.reserve(endpointToPlayerId_.size());
    for (const auto& [key, pid] : endpointToPlayerId_) {
        rtype::net::PlayerEntry pe{};
        pe.id = pid;
    // Clamp lives to 0..10 for network
    auto itl = playerLives_.find(pid);
    std::uint8_t lives = (itl != playerLives_.end()) ? itl->second : 0;
    if (lives > 10) lives = 10;
    pe.lives = lives;
        auto itn = playerNames_.find(pid);
        std::string name = (itn != playerNames_.end()) ? itn->second : (std::string("Player") + std::to_string(pid));
        // copy up to 15 chars + ensure null-termination
        std::memset(pe.name, 0, sizeof(pe.name));
        std::strncpy(pe.name, name.c_str(), sizeof(pe.name) - 1);
    entries.push_back(pe);
    }
    rh.count = static_cast<std::uint8_t>(entries.size());
    rtype::net::Header hdr{};
    hdr.version = rtype::net::ProtocolVersion;
    hdr.type = rtype::net::MsgType::Roster;
    hdr.size = static_cast<std::uint16_t>(sizeof(rh) + entries.size() * sizeof(rtype::net::PlayerEntry));
    std::vector<char> out(sizeof(hdr) + hdr.size);
    std::memcpy(out.data(), &hdr, sizeof(hdr));
    std::memcpy(out.data() + sizeof(hdr), &rh, sizeof(rh));
    if (!entries.empty())
        std::memcpy(out.data() + sizeof(hdr) + sizeof(rh), entries.data(), entries.size() * sizeof(rtype::net::PlayerEntry));
    for (const auto& [_, ep] : keyToEndpoint_)
        doSend(ep, out.data(), out.size());
}

void UdpServer::broadcastLivesUpdate(std::uint32_t id, std::uint8_t lives) {
    rtype::net::Header hdr{};
    hdr.version = rtype::net::ProtocolVersion;
    hdr.type = rtype::net::MsgType::LivesUpdate;
    hdr.size = sizeof(rtype::net::LivesUpdatePayload);
    rtype::net::LivesUpdatePayload p{ id, lives };
    std::vector<char> out(sizeof(hdr) + sizeof(p));
    std::memcpy(out.data(), &hdr, sizeof(hdr));
    std::memcpy(out.data() + sizeof(hdr), &p, sizeof(p));
    for (const auto& [_, ep] : keyToEndpoint_)
        doSend(ep, out.data(), out.size());
}

void UdpServer::doSend(const asio::ip::udp::endpoint& to, const void* data, std::size_t size) {
    auto buf = std::make_shared<std::vector<char>>(static_cast<const char*>(data), static_cast<const char*>(data) + size);
    socket_.async_send_to(asio::buffer(*buf), to, [buf](std::error_code, std::size_t) {});
}

}
