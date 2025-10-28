#include "gameplay/GameSession.hpp"
#include "protocol/TcpServer.hpp"
#include <iostream>
#include <cstring>
#include <cmath>
#include <chrono>
#include <thread>
#include "rt/game/Components.hpp"
#include "rt/game/Systems.hpp"

using namespace rtype::server::gameplay;
using rtype::server::TcpServer;

static std::string makeKeyLocal(const asio::ip::udp::endpoint& ep) {
    return ep.address().to_string() + ":" + std::to_string(ep.port());
}

GameSession::GameSession(asio::io_context& io, SendFn sendFn, TcpServer* tcpServer)
    : io_(io), send_(std::move(sendFn)), rng_(std::random_device{}()), tcp_(tcpServer) {}

GameSession::~GameSession() { stop(); }

void GameSession::start() {
    running_ = true;
    gameThread_ = std::thread([this]{ gameLoop(); });
}

void GameSession::stop() {
    running_ = false;
    if (gameThread_.joinable()) gameThread_.join();
}

void GameSession::onUdpPacket(const asio::ip::udp::endpoint& from, const char* data, std::size_t size) {
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
            reg_.emplace<rt::game::Transform>(e, rt::game::Transform{50.f, 100.f + static_cast<float>(endpointToPlayerId_.size()) * 40.f});
            reg_.emplace<rt::game::Velocity>(e, rt::game::Velocity{0.f, 0.f});
            reg_.emplace<rt::game::NetType>(e, rt::game::NetType{rtype::net::EntityType::Player});
            reg_.emplace<rt::game::ColorRGBA>(e, rt::game::ColorRGBA{0x55AAFFFFu});
            reg_.emplace<rt::game::PlayerInput>(e, rt::game::PlayerInput{0, 150.f});
            reg_.emplace<rt::game::Shooter>(e, rt::game::Shooter{0.f, 0.15f, 320.f});
            reg_.emplace<rt::game::ChargeGun>(e, rt::game::ChargeGun{0.f, 2.0f, false});
            reg_.emplace<rt::game::Size>(e, rt::game::Size{20.f, 12.f});
            reg_.emplace<rt::game::Score>(e, rt::game::Score{0});

            endpointToPlayerId_[key] = e;
            playerInputBits_[e] = 0;
            playerLives_[e] = 4;
            playerScores_[e] = 0;
            std::string uname;
            if (payloadSize > 0) {
                uname.assign(payload, payload + std::min<std::size_t>(payloadSize, 15));
                while (!uname.empty() && (uname.back() == '\0' || uname.back() == ' ')) uname.pop_back();
            }
            if (uname.empty()) uname = "Player" + std::to_string(e);
            playerNames_[e] = uname;

            broadcastRoster();
            maybeStartGame();
        }

        rtype::net::Header ack{0, rtype::net::MsgType::HelloAck, rtype::net::ProtocolVersion};
        send_(from, &ack, sizeof(ack));
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
        removeClient(key);
        return;
    }
}

void GameSession::gameLoop() {
    using clock = std::chrono::steady_clock;
    const double tickRate = 60.0;
    const double dt = 1.0 / tickRate;
    const double stateInterval = 1.0 / std::max(1.0, stateHz_);
    auto next = clock::now();
    float elapsed = 0.f;
    lastStateSend_ = clock::now();

    reg_.addSystem(std::make_unique<rt::game::InputSystem>());
    reg_.addSystem(std::make_unique<rt::game::ShootingSystem>());
    reg_.addSystem(std::make_unique<rt::game::ChargeShootingSystem>());
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
                    if (auto* t = reg_.get<rt::game::Transform>(e)) {
                        constexpr float kStartX = 50.f;
                        constexpr float kWorldH = 600.f;
                        constexpr float kTopMargin = 56.f;
                        constexpr float kBottomMargin = 10.f;
                        float y = t->y;
                        float maxY = kWorldH - kBottomMargin - 12.f;
                        if (y < kTopMargin) y = kTopMargin;
                        if (y > maxY) y = maxY;
                        t->x = kStartX; t->y = y;
                    }
                    if (auto* v = reg_.get<rt::game::Velocity>(e)) { v->vx = 0.f; v->vy = 0.f; }
                    if (auto* inv = reg_.get<rt::game::Invincible>(e)) {
                        inv->timeLeft = std::max(inv->timeLeft, 1.0f);
                    } else {
                        reg_.emplace<rt::game::Invincible>(e, rt::game::Invincible{1.0f});
                    }
                    hf->value = false;
                }
            }
        }

        std::int32_t teamScore = 0;
        for (auto& [e, inp] : reg_.storage<rt::game::PlayerInput>().data()) {
            (void)inp;
            if (auto* sc = reg_.get<rt::game::Score>(e)) {
                playerScores_[e] = sc->value;
                teamScore += sc->value;
            }
        }
        if (teamScore != lastTeamScore_) {
            lastTeamScore_ = teamScore;
            rtype::net::Header hdr{};
            hdr.version = rtype::net::ProtocolVersion;
            hdr.type = rtype::net::MsgType::ScoreUpdate;
            hdr.size = sizeof(rtype::net::ScoreUpdatePayload);
            rtype::net::ScoreUpdatePayload p{ 0, teamScore };
            std::vector<char> out(sizeof(hdr) + sizeof(p));
            std::memcpy(out.data(), &hdr, sizeof(hdr));
            std::memcpy(out.data() + sizeof(hdr), &p, sizeof(p));
            for (const auto& [_, ep] : keyToEndpoint_) send_(ep, out.data(), out.size());
        }

        checkTimeouts();

        auto now = clock::now();
        if (std::chrono::duration<double>(now - lastStateSend_).count() >= stateInterval) {
            broadcastState();
            lastStateSend_ = now;
        }

        std::this_thread::sleep_until(next);
    }
}

void GameSession::checkTimeouts() {
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

void GameSession::removeClient(const std::string& key) {
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
        send_(ep, out.data(), out.size());

    broadcastRoster();
    std::cout << "[server] Removed disconnected client: " << key << "\n";

    if (endpointToPlayerId_.size() > 0 && endpointToPlayerId_.size() < 2) {
        rtype::net::Header rtm{0, rtype::net::MsgType::ReturnToMenu, rtype::net::ProtocolVersion};
        for (auto& [_, ep] : keyToEndpoint_) {
            send_(ep, &rtm, sizeof(rtm));
        }
        gameStarted_ = false;
    }
}

void GameSession::broadcastState() {
    rtype::net::Header hdr{};
    hdr.version = rtype::net::ProtocolVersion;
    hdr.type = rtype::net::MsgType::State;

    constexpr std::size_t kMaxUdpBytes   = 1200;
    constexpr std::size_t kHeaderBytes   = sizeof(rtype::net::Header);
    constexpr std::size_t kStateHdrBytes = sizeof(rtype::net::StateHeader);
    constexpr std::size_t kEntBytes      = sizeof(rtype::net::PackedEntity);

    const std::size_t maxEntities = (kMaxUdpBytes > (kHeaderBytes + kStateHdrBytes))
        ? ((kMaxUdpBytes - (kHeaderBytes + kStateHdrBytes)) / kEntBytes)
        : 0;

    std::vector<rtype::net::PackedEntity> players;
    std::vector<rtype::net::PackedEntity> bullets;
    std::vector<rtype::net::PackedEntity> enemies;
    players.reserve(16); bullets.reserve(64); enemies.reserve(64);

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
        switch (nt.type) {
            case rtype::net::EntityType::Player: players.push_back(pe); break;
            case rtype::net::EntityType::Bullet: bullets.push_back(pe); break;
            case rtype::net::EntityType::Enemy:
            default: enemies.push_back(pe); break;
        }
    }

    std::vector<rtype::net::PackedEntity> net;
    net.reserve(std::min<std::size_t>(players.size() + bullets.size() + enemies.size(), maxEntities));
    auto appendLimited = [&](const std::vector<rtype::net::PackedEntity>& src) {
        for (const auto& pe : src) {
            if (net.size() >= maxEntities) break;
            net.push_back(pe);
        }
    };
    appendLimited(players); appendLimited(bullets); appendLimited(enemies);

    rtype::net::StateHeader sh{};
    sh.count = static_cast<std::uint16_t>(net.size());

    std::size_t payloadSize = sizeof(rtype::net::StateHeader) + net.size() * sizeof(rtype::net::PackedEntity);
    hdr.size = static_cast<std::uint16_t>(payloadSize);

    std::vector<char> out(sizeof(rtype::net::Header) + payloadSize);
    std::memcpy(out.data(), &hdr, sizeof(hdr));
    std::memcpy(out.data() + sizeof(hdr), &sh, sizeof(sh));
    auto* arr = reinterpret_cast<rtype::net::PackedEntity*>(out.data() + sizeof(hdr) + sizeof(sh));
    for (std::uint16_t i = 0; i < sh.count; ++i) arr[i] = net[i];

    for (const auto& [key, ep] : keyToEndpoint_)
        send_(ep, out.data(), out.size());
}

void GameSession::broadcastRoster() {
    rtype::net::RosterHeader rh{};
    std::vector<rtype::net::PlayerEntry> entries;
    entries.reserve(endpointToPlayerId_.size());

    for (const auto& [key, pid] : endpointToPlayerId_) {
        rtype::net::PlayerEntry pe{};
        pe.id = pid;

        auto itl = playerLives_.find(pid);
        std::uint8_t lives = (itl != playerLives_.end()) ? itl->second : 0;
        if (lives > 10) lives = 10;
        pe.lives = lives;

        auto itn = playerNames_.find(pid);
        std::string name = (itn != playerNames_.end()) ? itn->second : (std::string("Player") + std::to_string(pid));
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
        send_(ep, out.data(), out.size());
}

void GameSession::broadcastLivesUpdate(std::uint32_t id, std::uint8_t lives) {
    rtype::net::Header hdr{};
    hdr.version = rtype::net::ProtocolVersion;
    hdr.type = rtype::net::MsgType::LivesUpdate;
    hdr.size = sizeof(rtype::net::LivesUpdatePayload);
    rtype::net::LivesUpdatePayload p{ id, lives };
    std::vector<char> out(sizeof(hdr) + sizeof(p));
    std::memcpy(out.data(), &hdr, sizeof(hdr));
    std::memcpy(out.data() + sizeof(hdr), &p, sizeof(p));
    for (const auto& [_, ep] : keyToEndpoint_)
        send_(ep, out.data(), out.size());
}

void GameSession::maybeStartGame() {
    const std::size_t required = 2;
    if (!gameStarted_ && endpointToPlayerId_.size() >= required) {
        gameStarted_ = true;
        if (tcp_) {
            std::cout << "[server] Enough players joined (" << endpointToPlayerId_.size()
                      << "). Sending StartGame over TCP.\n";
            tcp_->broadcastStartGame();
        }
    }
}

std::string GameSession::makeKey(const asio::ip::udp::endpoint& ep) {
    return makeKeyLocal(ep);
}
