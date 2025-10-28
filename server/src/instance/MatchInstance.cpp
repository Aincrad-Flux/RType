#include "instance/MatchInstance.hpp"
#include "network/NetworkManager.hpp"
#include "rt/game/Components.hpp"
#include "rt/game/Systems.hpp"
#include <cstring>
#include <iostream>
#include <algorithm>

using namespace rtype::server::instance;
using rtype::server::net::NetworkManager;

static std::string makeKey(const asio::ip::udp::endpoint& ep) { return NetworkManager::makeKey(ep); }

MatchInstance::MatchInstance(NetworkManager& net)
    : rng_(std::random_device{}()), net_(net) {}

MatchInstance::~MatchInstance() { stop(); }

void MatchInstance::start() {
    running_ = true;
    // Initialize ECS systems with current lobby settings
    float elapsed = 0.f; // local in runLoop; pointer valid while runLoop runs
    (void)elapsed; // silence until assigned in runLoop
    gameThread_ = std::thread([this]{ runLoop(); });
}

void MatchInstance::stop() {
    running_ = false;
    if (gameThread_.joinable()) gameThread_.join();
}

void MatchInstance::handlePacket(const asio::ip::udp::endpoint& from, const char* data, std::size_t size) {
    if (size < sizeof(rtype::net::Header)) return;
    const auto* header = reinterpret_cast<const rtype::net::Header*>(data);
    if (header->version != rtype::net::ProtocolVersion) return;
    const char* payload = data + sizeof(rtype::net::Header);
    std::size_t payloadSize = size - sizeof(rtype::net::Header);
    auto key = makeKey(from);
    net_.noteSeen(key);

    switch (header->type) {
        case rtype::net::MsgType::Hello: std::cout << "[server] RX Hello from " << key << std::endl; break;
        case rtype::net::MsgType::Input: break;
        case rtype::net::MsgType::Disconnect: std::cout << "[server] RX Disconnect from " << key << std::endl; break;
        case rtype::net::MsgType::LobbyConfig: std::cout << "[server] RX LobbyConfig from " << key << std::endl; break;
        case rtype::net::MsgType::StartMatch: std::cout << "[server] RX StartMatch from " << key << std::endl; break;
        default: std::cout << "[server] RX type=" << (int)header->type << " from " << key << std::endl; break;
    }

    if (header->type == rtype::net::MsgType::Hello) {
        net_.registerEndpoint(key, from);
        auto& reg_ = session_.registry();
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
                while (!uname.empty() && (uname.back() == '\0' || uname.back() == ' ')) uname.pop_back();
            }
            if (uname.empty()) uname = "Player" + std::to_string(e);
            playerNames_[e] = uname;
            broadcastRoster();
            if (hostId_ == 0) hostId_ = e;
            broadcastLobbyStatus();
            std::cout << "[server] Player joined: id=" << e << " name='" << playerNames_[e]
                      << "' totalPlayers=" << endpointToPlayerId_.size() << std::endl;
        }
        rtype::net::Header ack{0, rtype::net::MsgType::HelloAck, rtype::net::ProtocolVersion};
        net_.send(from, &ack, sizeof(ack));
        return;
    }

    if (header->type == rtype::net::MsgType::Input) {
        if (payloadSize >= sizeof(rtype::net::InputPacket)) {
            auto* in = reinterpret_cast<const rtype::net::InputPacket*>(payload);
            auto it = endpointToPlayerId_.find(key);
            if (it != endpointToPlayerId_.end()) {
                playerInputBits_[it->second] = in->bits;
                auto& reg_ = session_.registry();
                if (auto* pi = reg_.get<rt::game::PlayerInput>(it->second)) pi->bits = in->bits;
            }
        }
        return;
    }

    if (header->type == rtype::net::MsgType::Disconnect) {
        removeClient(key);
        return;
    }

    if (header->type == rtype::net::MsgType::LobbyConfig) {
        auto it = endpointToPlayerId_.find(key);
        if (it != endpointToPlayerId_.end() && it->second == hostId_) {
            if (payloadSize >= sizeof(rtype::net::LobbyConfigPayload)) {
                auto* cfg = reinterpret_cast<const rtype::net::LobbyConfigPayload*>(payload);
                lobbyBaseLives_ = std::clamp<std::uint8_t>(cfg->baseLives, 1, (std::uint8_t)6);
                lobbyDifficulty_ = std::clamp<std::uint8_t>(cfg->difficulty, (std::uint8_t)0, (std::uint8_t)2);
                switch (lobbyDifficulty_) {
                    case 0: shooterPercent_ = 15; break;
                    case 1: shooterPercent_ = 35; break;
                    case 2: shooterPercent_ = 60; break;
                    default: shooterPercent_ = 25; break;
                }
                if (spawnSys_) { spawnSys_->setDifficulty(lobbyDifficulty_); spawnSys_->setShooterPercent(shooterPercent_); }
                if (!matchStarted_) {
                    for (auto& [_, pid] : endpointToPlayerId_) playerLives_[pid] = lobbyBaseLives_;
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
            if (endpointToPlayerId_.size() >= 2) {
                auto& reg_ = session_.registry();
                std::vector<rt::ecs::Entity> toDestroy;
                for (auto& [e, nt] : reg_.storage<rt::game::NetType>().data()) {
                    if (nt.type != rtype::net::EntityType::Player) toDestroy.push_back(e);
                }
                for (auto& [e, f] : reg_.storage<rt::game::Formation>().data()) { (void)f; toDestroy.push_back(e); }
                for (auto e : toDestroy) { try { reg_.destroy(e); } catch (...) {} }
                for (auto& [key2, pid] : endpointToPlayerId_) {
                    (void)key2;
                    playerLives_[pid] = std::clamp<std::uint8_t>(lobbyBaseLives_, 1, (std::uint8_t)6);
                    playerScores_[pid] = 0;
                    if (auto* t = reg_.get<rt::game::Transform>(pid)) { t->x = 50.f; }
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

void MatchInstance::runLoop() {
    using clock = std::chrono::steady_clock;
    const double tickRate = 60.0;
    const double dt = 1.0 / tickRate;
    const double stateInterval = 1.0 / std::max(1.0, stateHz_);
    auto next = clock::now();
    float elapsed = 0.f;
    lastStateSend_ = clock::now();
    std::cout << "[server] Game loop started, tickRate=" << tickRate << "Hz" << std::endl;

    // Initialize ECS systems once
    session_.initSystems(rng_, &elapsed, lobbyDifficulty_, shooterPercent_, &spawnSys_);

    auto lastDiag = clock::now();
    bool prevActive = false;
    while (running_) {
        next += std::chrono::duration_cast<clock::duration>(std::chrono::duration<double>(dt));
        bool active = matchStarted_;
        if (active != prevActive) {
            std::cout << "[server] Game state -> " << (active ? "ACTIVE (started)" : "WAITING (not started)") << std::endl;
            if (!active) {
                auto& reg_ = session_.registry();
                std::vector<rt::ecs::Entity> toDestroy;
                for (auto& [e, nt] : reg_.storage<rt::game::NetType>().data()) {
                    if (nt.type != rtype::net::EntityType::Player) toDestroy.push_back(e);
                }
                for (auto& [e, f] : reg_.storage<rt::game::Formation>().data()) { (void)f; toDestroy.push_back(e); }
                for (auto e : toDestroy) { try { session_.registry().destroy(e); } catch (...) {} }
                broadcastLobbyStatus();
            }
            prevActive = active;
        }

        if (active) {
            elapsed += static_cast<float>(dt);
            session_.update(static_cast<float>(dt));
        }
        if (active) {
            auto nowDiag = clock::now();
            if (std::chrono::duration_cast<std::chrono::milliseconds>(nowDiag - lastDiag).count() > 1000) {
                std::size_t players = 0, enemies = 0, bullets = 0, formations = 0;
                auto& reg_ = session_.registry();
                for (auto& [e, nt] : reg_.storage<rt::game::NetType>().data()) {
                    (void)e;
                    switch (nt.type) {
                        case rtype::net::EntityType::Player: ++players; break;
                        case rtype::net::EntityType::Enemy:  ++enemies; break;
                        case rtype::net::EntityType::Bullet: ++bullets; break;
                    }
                }
                for (auto& [e, f] : reg_.storage<rt::game::Formation>().data()) { (void)e; (void)f; ++formations; }
                std::cout << "[server] Diag: players=" << players
                          << " enemies=" << enemies
                          << " bullets=" << bullets
                          << " formations=" << formations << std::endl;
                lastDiag = nowDiag;
            }
        }

        // After systems, handle player hits for lives/respawn
        if (active) {
            auto& reg_ = session_.registry();
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
                        if (playerLives_[e] == 0) {
                            try { reg_.destroy(e); } catch (...) {}
                            rtype::net::Header hdr{}; hdr.version = rtype::net::ProtocolVersion; hdr.type = rtype::net::MsgType::Despawn; hdr.size = sizeof(std::uint32_t);
                            std::vector<char> out(sizeof(hdr) + sizeof(std::uint32_t));
                            std::memcpy(out.data(), &hdr, sizeof(hdr));
                            std::memcpy(out.data() + sizeof(hdr), &e, sizeof(e));
                            net_.broadcast(out.data(), out.size());
                        } else {
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
                            if (auto* inv = reg_.get<rt::game::Invincible>(e)) { inv->timeLeft = std::max(inv->timeLeft, 1.0f); }
                            else { reg_.emplace<rt::game::Invincible>(e, rt::game::Invincible{1.0f}); }
                        }
                        hf->value = false;
                    }
                }
            }
        }

        // Score aggregation
        std::int32_t teamScore = 0;
        {
            auto& reg_ = session_.registry();
            for (auto& [e, inp] : reg_.storage<rt::game::PlayerInput>().data()) {
                (void)inp;
                if (auto* sc = reg_.get<rt::game::Score>(e)) {
                    playerScores_[e] = sc->value;
                    teamScore += sc->value;
                }
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
            net_.broadcast(out.data(), out.size());
        }

        if (active) {
            int alive = 0;
            for (auto& [pid, lives] : playerLives_) { (void)pid; if (lives > 0) ++alive; }
            if (alive == 0 && !endpointToPlayerId_.empty()) {
                matchStarted_ = false;
                broadcastGameOver(0);
                broadcastLobbyStatus();
                std::cout << "[server] All players eliminated. Match ended." << std::endl;
            }
        }

        // Network housekeeping
        net_.checkTimeouts([this](const std::string& k){ this->removeClient(k); });

        // State broadcast throttle
        auto now = clock::now();
        if (std::chrono::duration<double>(now - lastStateSend_).count() >= stateInterval) {
            broadcastState();
            lastStateSend_ = now;
        }
        std::this_thread::sleep_until(next);
    }
}

void MatchInstance::removeClient(const std::string& key) {
    auto it = endpointToPlayerId_.find(key);
    if (it == endpointToPlayerId_.end()) return;
    auto id = it->second;
    endpointToPlayerId_.erase(it);
    // Network manager will be cleaned in its timeout pass; but ensure endpoint is dropped
    net_.unregisterEndpoint(key);
    playerInputBits_.erase(id);
    try { session_.registry().destroy(id); } catch (...) {}
    rtype::net::Header hdr{};
    hdr.size = sizeof(std::uint32_t);
    hdr.type = rtype::net::MsgType::Despawn;
    hdr.version = rtype::net::ProtocolVersion;
    std::vector<char> out(sizeof(hdr) + sizeof(std::uint32_t));
    std::memcpy(out.data(), &hdr, sizeof(hdr));
    std::memcpy(out.data() + sizeof(hdr), &id, sizeof(id));
    net_.broadcast(out.data(), out.size());
    broadcastRoster();
    std::cout << "[server] Removed disconnected client: " << key << "\n";

    if (hostId_ == id) {
        hostId_ = 0;
        for (auto& [k2, pid2] : endpointToPlayerId_) { (void)k2; hostId_ = pid2; break; }
    }
    if (endpointToPlayerId_.size() < 2) {
        if (matchStarted_) {
            matchStarted_ = false;
            broadcastLobbyStatus();
        }
        if (!endpointToPlayerId_.empty()) {
            rtype::net::Header rtm{0, rtype::net::MsgType::ReturnToMenu, rtype::net::ProtocolVersion};
            net_.broadcast(&rtm, sizeof(rtm));
        }
    }
    if (!endpointToPlayerId_.empty()) broadcastLobbyStatus();
}

void MatchInstance::broadcastState() {
    auto& reg_ = session_.registry();
    rtype::net::Header hdr{};
    hdr.version = rtype::net::ProtocolVersion;
    hdr.type = rtype::net::MsgType::State;
    rtype::net::StateHeader sh{};
    constexpr std::size_t kMaxUdpBytes = 1200;
    constexpr std::size_t kHeaderBytes = sizeof(rtype::net::Header);
    constexpr std::size_t kStateHdrBytes = sizeof(rtype::net::StateHeader);
    constexpr std::size_t kEntBytes = sizeof(rtype::net::PackedEntity);
    const std::size_t maxEntities = (kMaxUdpBytes > (kHeaderBytes + kStateHdrBytes))
        ? ((kMaxUdpBytes - (kHeaderBytes + kStateHdrBytes)) / kEntBytes)
        : 0;

    std::vector<rtype::net::PackedEntity> players;
    std::vector<rtype::net::PackedEntity> bullets;
    std::vector<rtype::net::PackedEntity> enemies;
    players.reserve(16);
    bullets.reserve(64);
    enemies.reserve(64);
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
            case rtype::net::EntityType::Enemy: default: enemies.push_back(pe); break;
        }
    }
    std::vector<rtype::net::PackedEntity> netents;
    netents.reserve(std::min<std::size_t>(players.size() + bullets.size() + enemies.size(), maxEntities));
    auto appendLimited = [&](const std::vector<rtype::net::PackedEntity>& src) {
        for (const auto& pe : src) {
            if (netents.size() >= maxEntities) break;
            netents.push_back(pe);
        }
    };
    appendLimited(players);
    appendLimited(bullets);
    appendLimited(enemies);

    sh.count = static_cast<std::uint16_t>(netents.size());
    std::size_t payloadSize = sizeof(rtype::net::StateHeader) + netents.size() * sizeof(rtype::net::PackedEntity);
    hdr.size = static_cast<std::uint16_t>(payloadSize);
    std::vector<char> out(sizeof(rtype::net::Header) + payloadSize);
    std::memcpy(out.data(), &hdr, sizeof(hdr));
    std::memcpy(out.data() + sizeof(hdr), &sh, sizeof(sh));
    auto* arr = reinterpret_cast<rtype::net::PackedEntity*>(out.data() + sizeof(hdr) + sizeof(sh));
    for (std::uint16_t i = 0; i < sh.count; ++i) arr[i] = netents[i];
    net_.broadcast(out.data(), out.size());
}

void MatchInstance::broadcastRoster() {
    rtype::net::RosterHeader rh{};
    std::vector<rtype::net::PlayerEntry> entries;
    entries.reserve(endpointToPlayerId_.size());
    for (const auto& [key, pid] : endpointToPlayerId_) {
        (void)key;
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
    net_.broadcast(out.data(), out.size());
}

void MatchInstance::broadcastLivesUpdate(std::uint32_t id, std::uint8_t lives) {
    rtype::net::Header hdr{};
    hdr.version = rtype::net::ProtocolVersion;
    hdr.type = rtype::net::MsgType::LivesUpdate;
    hdr.size = sizeof(rtype::net::LivesUpdatePayload);
    rtype::net::LivesUpdatePayload p{ id, lives };
    std::vector<char> out(sizeof(hdr) + sizeof(p));
    std::memcpy(out.data(), &hdr, sizeof(hdr));
    std::memcpy(out.data() + sizeof(hdr), &p, sizeof(p));
    net_.broadcast(out.data(), out.size());
}

void MatchInstance::broadcastScoreUpdate(std::uint32_t id, std::int32_t score) {
    rtype::net::Header hdr{};
    hdr.version = rtype::net::ProtocolVersion;
    hdr.type = rtype::net::MsgType::ScoreUpdate;
    hdr.size = sizeof(rtype::net::ScoreUpdatePayload);
    rtype::net::ScoreUpdatePayload p{ id, score };
    std::vector<char> out(sizeof(hdr) + sizeof(p));
    std::memcpy(out.data(), &hdr, sizeof(hdr));
    std::memcpy(out.data() + sizeof(hdr), &p, sizeof(p));
    net_.broadcast(out.data(), out.size());
}

void MatchInstance::broadcastLobbyStatus() {
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
    net_.broadcast(out.data(), out.size());
}

void MatchInstance::broadcastGameOver(std::uint8_t reason) {
    rtype::net::Header hdr{};
    hdr.version = rtype::net::ProtocolVersion;
    hdr.type = rtype::net::MsgType::GameOver;
    hdr.size = sizeof(rtype::net::GameOverPayload);
    rtype::net::GameOverPayload p{reason};
    std::array<char, sizeof(hdr) + sizeof(p)> out{};
    std::memcpy(out.data(), &hdr, sizeof(hdr));
    std::memcpy(out.data() + sizeof(hdr), &p, sizeof(p));
    net_.broadcast(out.data(), out.size());
}
