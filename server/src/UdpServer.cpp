#include "UdpServer.hpp"
#include "TcpServer.hpp"
#include <iostream>
#include <cstring>
#include <cmath>
#include <chrono>
#include <thread>
#include "rt/game/Components.hpp"
#include "rt/game/Systems.hpp"

using namespace rtype::server;

static std::string makeKey(const asio::ip::udp::endpoint& ep) {
    return ep.address().to_string() + ":" + std::to_string(ep.port());
}

UdpServer::UdpServer(asio::io_context& io, unsigned short port)
    : io_(io)
    , socket_(io_, asio::ip::udp::endpoint(asio::ip::udp::v4(), port))
    , rng_(std::random_device{}())
{
}

UdpServer::~UdpServer() { stop(); }

void UdpServer::start() {
    std::cout << "[server] Listening UDP on " << socket_.local_endpoint().port() << "\n";
    running_ = true;
    doReceive();                       // begin async receive on io_
    gameThread_ = std::thread([this]{  // keep game loop in its own thread
        gameLoop();
    });
}

void UdpServer::stop() {
    running_ = false;

    if (socket_.is_open()) {
        asio::error_code ec; socket_.close(ec);
    }
    if (gameThread_.joinable()) gameThread_.join();
}

void UdpServer::doReceive() {
    socket_.async_receive_from(
        asio::buffer(buffer_), remote_,
        [this](std::error_code ec, std::size_t n) {
            if (!ec && n >= sizeof(rtype::net::Header))
                handlePacket(remote_, buffer_.data(), n);
            if (running_) doReceive();
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
    // Debug: trace incoming packet types
    switch (header->type) {
        case rtype::net::MsgType::Hello: std::cout << "[server] RX Hello from " << key << std::endl; break;
        case rtype::net::MsgType::Input: break; // too chatty
        case rtype::net::MsgType::Disconnect: std::cout << "[server] RX Disconnect from " << key << std::endl; break;
        case rtype::net::MsgType::LobbyConfig: std::cout << "[server] RX LobbyConfig from " << key << std::endl; break;
        case rtype::net::MsgType::StartMatch: std::cout << "[server] RX StartMatch from " << key << std::endl; break;
        default: std::cout << "[server] RX type=" << (int)header->type << " from " << key << std::endl; break;
    }

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
            // Default lives
            std::uint8_t base = std::clamp<std::uint8_t>(lobbyBaseLives_, 1, (std::uint8_t)6);
            playerLives_[e] = base;
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

            // NEW: maybe trigger StartGame if enough players (TCP)
            maybeStartGame();
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

    if (header->type == rtype::net::MsgType::LobbyConfig) {
        // Only host can change lobby settings
        auto it = endpointToPlayerId_.find(key);
        if (it != endpointToPlayerId_.end() && it->second == hostId_) {
            if (payloadSize >= sizeof(rtype::net::LobbyConfigPayload)) {
                auto* cfg = reinterpret_cast<const rtype::net::LobbyConfigPayload*>(payload);
                lobbyBaseLives_ = std::clamp<std::uint8_t>(cfg->baseLives, 1, (std::uint8_t)6);
                lobbyDifficulty_ = std::clamp<std::uint8_t>(cfg->difficulty, (std::uint8_t)0, (std::uint8_t)2);
                // map difficulty -> shooter percent
                switch (lobbyDifficulty_) {
                    case 0: shooterPercent_ = 15; break; // Easy
                    case 1: shooterPercent_ = 35; break; // Normal
                    case 2: shooterPercent_ = 60; break; // Hard
                    default: shooterPercent_ = 25; break;
                }
                if (spawnSys_) { spawnSys_->setDifficulty(lobbyDifficulty_); spawnSys_->setShooterPercent(shooterPercent_); }
                // If match not started, update lives for connected players to reflect base
                if (!matchStarted_) {
                    for (auto& [_, pid] : endpointToPlayerId_) {
                        playerLives_[pid] = lobbyBaseLives_;
                    }
                    broadcastRoster();
                }
                broadcastLobbyStatus();
            }
        }
        return;
    }

    if (header->type == rtype::net::MsgType::StartMatch) {
        auto it = endpointToPlayerId_.find(key);
        if (it != endpointToPlayerId_.end() && it->second == hostId_) {
            // Require at least 2 connected players to start
            if (endpointToPlayerId_.size() >= 2) {
                // Reset world (remove enemies/bullets/formations)
                std::vector<rt::ecs::Entity> toDestroy;
                for (auto& [e, nt] : reg_.storage<rt::game::NetType>().data()) {
                    if (nt.type != rtype::net::EntityType::Player) toDestroy.push_back(e);
                }
                for (auto& [e, f] : reg_.storage<rt::game::Formation>().data()) { (void)f; toDestroy.push_back(e); }
                for (auto e : toDestroy) { try { reg_.destroy(e); } catch (...) {} }
                // Reset players: positions, velocities, scores, lives from lobby
                for (auto& [key2, pid] : endpointToPlayerId_) {
                    (void)key2;
                    playerLives_[pid] = std::clamp<std::uint8_t>(lobbyBaseLives_, 1, (std::uint8_t)6);
                    playerScores_[pid] = 0;
                    if (auto* t = reg_.get<rt::game::Transform>(pid)) { t->x = 50.f; /* keep Y */ }
                    if (auto* v = reg_.get<rt::game::Velocity>(pid)) { v->vx = 0.f; v->vy = 0.f; }
                }
                lastTeamScore_ = 0;
                matchStarted_ = true;
                broadcastRoster();
                broadcastLobbyStatus();
                std::cout << "[server] Match started by host id=" << hostId_ << std::endl;
            }
        }
        return;
    }
}

void UdpServer::gameLoop() {
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
    {
        auto sys = std::make_unique<rt::game::FormationSpawnSystem>(rng_, &elapsed);
        spawnSys_ = sys.get();
        // apply initial lobby settings
        spawnSys_->setDifficulty(lobbyDifficulty_);
        spawnSys_->setShooterPercent(shooterPercent_);
        reg_.addSystem(std::move(sys));
    }
    auto lastDiag = clock::now();

    bool prevActive = false;
    while (running_) {
        next += std::chrono::duration_cast<clock::duration>(std::chrono::duration<double>(dt));
        elapsed += static_cast<float>(dt);
        reg_.update(static_cast<float>(dt));

        // Handle player hits -> decrement lives and respawn
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

        // Team score
        std::int32_t teamScore = 0;
        for (auto& [e, inp] : reg_.storage<rt::game::PlayerInput>().data()) {
            (void)inp;
            if (auto* sc = reg_.get<rt::game::Score>(e)) {
                playerScores_[e] = sc->value;
                teamScore += sc->value;
            }
        }
        if (active && teamScore != lastTeamScore_) {
            lastTeamScore_ = teamScore;
            rtype::net::Header hdr{};
            hdr.version = rtype::net::ProtocolVersion;
            hdr.type = rtype::net::MsgType::ScoreUpdate;
            hdr.size = sizeof(rtype::net::ScoreUpdatePayload);
            rtype::net::ScoreUpdatePayload p{ 0, teamScore };
            std::vector<char> out(sizeof(hdr) + sizeof(p));
            std::memcpy(out.data(), &hdr, sizeof(hdr));
            std::memcpy(out.data() + sizeof(hdr), &p, sizeof(p));
            for (const auto& [_, ep] : keyToEndpoint_) doSend(ep, out.data(), out.size());
        }

        checkTimeouts();

        // world state broadcast (throttled)
        auto now = clock::now();
        if (std::chrono::duration<double>(now - lastStateSend_).count() >= stateInterval) {
            broadcastState();
            lastStateSend_ = now;
        }

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

    // Update roster
    broadcastRoster();
    std::cout << "[server] Removed disconnected client: " << key << "\n";

    // If less than 2 players remain, notify and reset gameStarted_
    if (endpointToPlayerId_.size() > 0 && endpointToPlayerId_.size() < 2) {
        rtype::net::Header rtm{0, rtype::net::MsgType::ReturnToMenu, rtype::net::ProtocolVersion};
        for (auto& [_, ep] : keyToEndpoint_) {
            doSend(ep, &rtm, sizeof(rtm));
        }
        gameStarted_ = false; // allow a new StartGame when we reach threshold again
    }
    if (!endpointToPlayerId_.empty()) broadcastLobbyStatus();
}

void UdpServer::broadcastState() {
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
        doSend(ep, out.data(), out.size());
}

void UdpServer::broadcastRoster() {
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

void UdpServer::broadcastLobbyStatus() {
    rtype::net::Header hdr{};
    hdr.version = rtype::net::ProtocolVersion;
    hdr.type = rtype::net::MsgType::LobbyStatus;
    hdr.size = sizeof(rtype::net::LobbyStatusPayload);
    rtype::net::LobbyStatusPayload p{};
    p.hostId = hostId_;
    p.baseLives = std::clamp<std::uint8_t>(lobbyBaseLives_, 1, (std::uint8_t)6);
    p.difficulty = std::clamp<std::uint8_t>(lobbyDifficulty_, (std::uint8_t)0, (std::uint8_t)2);
    p.started = matchStarted_ ? 1 : 0;
    std::array<char, sizeof(hdr) + sizeof(p)> out{};
    std::memcpy(out.data(), &hdr, sizeof(hdr));
    std::memcpy(out.data() + sizeof(hdr), &p, sizeof(p));
    for (const auto& [_, ep] : keyToEndpoint_) doSend(ep, out.data(), out.size());
}

void UdpServer::broadcastGameOver(std::uint8_t reason) {
    rtype::net::Header hdr{};
    hdr.version = rtype::net::ProtocolVersion;
    hdr.type = rtype::net::MsgType::GameOver;
    hdr.size = sizeof(rtype::net::GameOverPayload);
    rtype::net::GameOverPayload p{reason};
    std::array<char, sizeof(hdr) + sizeof(p)> out{};
    std::memcpy(out.data(), &hdr, sizeof(hdr));
    std::memcpy(out.data() + sizeof(hdr), &p, sizeof(p));
    for (const auto& [_, ep] : keyToEndpoint_) doSend(ep, out.data(), out.size());
}

void UdpServer::doSend(const asio::ip::udp::endpoint& to, const void* data, std::size_t size) {
    auto buf = std::make_shared<std::vector<char>>(static_cast<const char*>(data), static_cast<const char*>(data) + size);
    socket_.async_send_to(asio::buffer(*buf), to, [buf](std::error_code, std::size_t) {});
}

void UdpServer::maybeStartGame() {
    // Option C: configurable later; use 2 players for now
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
