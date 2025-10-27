#include "Screens.hpp"
#include "common/Protocol.hpp"
#include <algorithm>
#include <cstring>
#include <chrono>
#include <iostream>

namespace client { namespace ui {

void Screens::handleNetPacket(const char* data, std::size_t n) {
    if (!data || n < sizeof(rtype::net::Header)) return;
    const auto* h = reinterpret_cast<const rtype::net::Header*>(data);
    if (h->version != rtype::net::ProtocolVersion) return;
    if (h->type == rtype::net::MsgType::State) {
        const char* p = data + sizeof(rtype::net::Header);
        if (n < sizeof(rtype::net::Header) + sizeof(rtype::net::StateHeader)) return;
        auto* sh = reinterpret_cast<const rtype::net::StateHeader*>(p);
        p += sizeof(rtype::net::StateHeader);
        std::size_t count = sh->count;
        if (n < sizeof(rtype::net::Header) + sizeof(rtype::net::StateHeader) + count * sizeof(rtype::net::PackedEntity)) return;
        _entities.clear();
        _entities.reserve(count);
        auto* arr = reinterpret_cast<const rtype::net::PackedEntity*>(p);
        std::size_t players = 0, enemies = 0, bullets = 0;
        for (std::size_t i = 0; i < count; ++i) {
            PackedEntity e{};
            e.id = arr[i].id;
            e.type = static_cast<unsigned char>(arr[i].type);
            e.x = arr[i].x; e.y = arr[i].y; e.vx = arr[i].vx; e.vy = arr[i].vy;
            e.rgba = arr[i].rgba;
            _entities.push_back(e);
            if (e.type == 1) ++players; else if (e.type == 2) ++enemies; else if (e.type == 3) ++bullets;
        }
        // Debug print: throttle to ~1/sec
        static auto last = std::chrono::steady_clock::now();
        auto now = std::chrono::steady_clock::now();
        if (std::chrono::duration_cast<std::chrono::milliseconds>(now - last).count() > 1000) {
            std::cout << "[client] State: total=" << count
                      << " players=" << players
                      << " enemies=" << enemies
                      << " bullets=" << bullets << std::endl;
            last = now;
        }
    } else if (h->type == rtype::net::MsgType::Roster) {
        const char* p = data + sizeof(rtype::net::Header);
        if (n < sizeof(rtype::net::Header) + sizeof(rtype::net::RosterHeader)) return;
        auto* rh = reinterpret_cast<const rtype::net::RosterHeader*>(p);
        p += sizeof(rtype::net::RosterHeader);
        std::size_t count = rh->count;
        if (n < sizeof(rtype::net::Header) + sizeof(rtype::net::RosterHeader) + count * sizeof(rtype::net::PlayerEntry)) return;
        _otherPlayers.clear();
        std::string unameTrunc = _username.substr(0, 15);
        for (std::size_t i = 0; i < count; ++i) {
            auto* pe = reinterpret_cast<const rtype::net::PlayerEntry*>(p + i * sizeof(rtype::net::PlayerEntry));
            std::string name(pe->name, pe->name + strnlen(pe->name, sizeof(pe->name)));
            int lives = std::clamp<int>(pe->lives, 0, 10);
            if (name == unameTrunc) { _playerLives = lives; _selfId = pe->id; continue; }
            _otherPlayers.push_back({pe->id, name, lives});
        }
        if (_otherPlayers.size() > 3) _otherPlayers.resize(3);
    } else if (h->type == rtype::net::MsgType::LivesUpdate) {
        const char* p = data + sizeof(rtype::net::Header);
        if (n < sizeof(rtype::net::Header) + sizeof(rtype::net::LivesUpdatePayload)) return;
        auto* lu = reinterpret_cast<const rtype::net::LivesUpdatePayload*>(p);
        unsigned id = lu->id;
        int lives = std::clamp<int>(lu->lives, 0, 10);
        if (id == _selfId) { _playerLives = lives; _gameOver = (_playerLives <= 0); }
        else { for (auto& op : _otherPlayers) { if (op.id == id) { op.lives = lives; break; } } }
    } else if (h->type == rtype::net::MsgType::ScoreUpdate) {
        const char* p = data + sizeof(rtype::net::Header);
        if (n < sizeof(rtype::net::Header) + sizeof(rtype::net::ScoreUpdatePayload)) return;
        auto* su = reinterpret_cast<const rtype::net::ScoreUpdatePayload*>(p);
        _score = su->score;
    } else if (h->type == rtype::net::MsgType::ReturnToMenu) {
        _serverReturnToMenu = true;
    }
}

} } // namespace client::ui
