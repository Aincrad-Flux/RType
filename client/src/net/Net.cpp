#include "Screens.hpp"
#include <asio.hpp>
#include <vector>
#include <cstring>
#include <array>
#include <thread>
#include <chrono>
#include "common/Protocol.hpp"

namespace client { namespace ui {

namespace {
    struct UdpClientGlobals {
        std::unique_ptr<asio::io_context> io;
        std::unique_ptr<asio::ip::udp::socket> sock;
        asio::ip::udp::endpoint server;
    } g;
}

void Screens::leaveSession() {
    teardownNet();
    _connected = false;
    _entities.clear();
    _serverReturnToMenu = false;
}

void Screens::ensureNetSetup() {
    if (g.io) return;
    g.io = std::make_unique<asio::io_context>();
    g.sock = std::make_unique<asio::ip::udp::socket>(*g.io);
    g.sock->open(asio::ip::udp::v4());
    asio::ip::udp::resolver resolver(*g.io);
    g.server = *resolver.resolve(asio::ip::udp::v4(), _serverAddr, _serverPort).begin();
    g.sock->non_blocking(true);
    _serverReturnToMenu = false;
    rtype::net::Header hdr{};
    hdr.version = rtype::net::ProtocolVersion;
    hdr.type = rtype::net::MsgType::Hello;
    hdr.size = static_cast<std::uint16_t>(_username.size());
    std::vector<char> out(sizeof(rtype::net::Header) + _username.size());
    std::memcpy(out.data(), &hdr, sizeof(hdr));
    if (!_username.empty()) std::memcpy(out.data() + sizeof(hdr), _username.data(), _username.size());
    g.sock->send_to(asio::buffer(out), g.server);
}

void Screens::sendDisconnect() {
    if (!g.sock) return;
    rtype::net::Header hdr{};
    hdr.version = rtype::net::ProtocolVersion;
    hdr.type = rtype::net::MsgType::Disconnect;
    hdr.size = 0;
    std::array<char, sizeof(rtype::net::Header)> buf{};
    std::memcpy(buf.data(), &hdr, sizeof(hdr));
    asio::error_code ec;
    g.sock->send_to(asio::buffer(buf), g.server, 0, ec);
}

void Screens::teardownNet() {
    if (g.sock && g.sock->is_open()) {
        sendDisconnect();
        asio::error_code ec; g.sock->close(ec);
    }
    g.sock.reset();
    g.io.reset();
    _spriteRowById.clear();
    _nextSpriteRow = 0;
}

void Screens::sendInput(std::uint8_t bits) {
    if (!g.sock) return;
    rtype::net::Header hdr{};
    hdr.version = rtype::net::ProtocolVersion;
    hdr.type = rtype::net::MsgType::Input;
    rtype::net::InputPacket ip{}; ip.sequence = 0; ip.bits = bits;
    hdr.size = sizeof(ip);
    std::array<char, sizeof(hdr) + sizeof(ip)> buf{};
    std::memcpy(buf.data(), &hdr, sizeof(hdr));
    std::memcpy(buf.data() + sizeof(hdr), &ip, sizeof(ip));
    g.sock->send_to(asio::buffer(buf), g.server);
}

void Screens::sendLobbyConfig(std::uint8_t difficulty, std::uint8_t baseLives) {
    if (!g.sock) return;
    rtype::net::Header hdr{};
    hdr.version = rtype::net::ProtocolVersion;
    hdr.type = rtype::net::MsgType::LobbyConfig;
    rtype::net::LobbyConfigPayload p{ baseLives, difficulty };
    hdr.size = sizeof(p);
    std::array<char, sizeof(hdr) + sizeof(p)> buf{};
    std::memcpy(buf.data(), &hdr, sizeof(hdr));
    std::memcpy(buf.data() + sizeof(hdr), &p, sizeof(p));
    g.sock->send_to(asio::buffer(buf), g.server);
}

void Screens::sendStartMatch() {
    if (!g.sock) return;
    rtype::net::Header hdr{};
    hdr.version = rtype::net::ProtocolVersion;
    hdr.type = rtype::net::MsgType::StartMatch;
    hdr.size = 0;
    std::array<char, sizeof(rtype::net::Header)> buf{};
    std::memcpy(buf.data(), &hdr, sizeof(hdr));
    g.sock->send_to(asio::buffer(buf), g.server);
}

void Screens::pumpNetworkOnce() {
    if (!g.sock) return;
    for (int i = 0; i < 8; ++i) {
        asio::ip::udp::endpoint from;
        std::array<char, 8192> in{};
        asio::error_code ec;
        std::size_t n = g.sock->receive_from(asio::buffer(in), from, 0, ec);
        if (ec == asio::error::would_block) break;
        if (ec || n < sizeof(rtype::net::Header)) break;
        handleNetPacket(in.data(), n);
    }
}

bool Screens::waitHelloAck(double timeoutSec) {
    double start = GetTime();
    while (GetTime() - start < timeoutSec) {
        asio::ip::udp::endpoint from;
        std::array<char, 1024> in{};
        asio::error_code ec;
        std::size_t n = g.sock->receive_from(asio::buffer(in), from, 0, ec);
        if (!ec && n >= sizeof(rtype::net::Header)) {
            auto* rh = reinterpret_cast<rtype::net::Header*>(in.data());
            if (rh->version == rtype::net::ProtocolVersion) {
                if (rh->type == rtype::net::MsgType::HelloAck) return true;
                handleNetPacket(in.data(), n);
            }
        } else if (ec && ec != asio::error::would_block) {
            logMessage(std::string("Receive error: ") + ec.message(), "ERROR");
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    return false;
}

bool Screens::autoConnect(ScreenState& screen, MultiplayerForm& form) {
    bool canConnect = !form.username.empty() && !form.serverAddress.empty() && !form.serverPort.empty();
    if (!canConnect) {
        _statusMessage = std::string("Missing host/port/name for autoconnect.");
        return false;
    }
    try {
        _username = form.username;
        _serverAddr = form.serverAddress;
        _serverPort = form.serverPort;
        _selfId = 0;
        _playerLives = 4;
        _gameOver = false;
        _otherPlayers.clear();
        teardownNet();
        ensureNetSetup();
        bool ok = waitHelloAck(1.0);
        if (ok) {
            _statusMessage = std::string("Player Connected.");
            _connected = true;
            screen = ScreenState::Waiting;
            return true;
        } else {
            _statusMessage = std::string("Connection failed.");
            teardownNet();
            return false;
        }
    } catch (const std::exception& e) {
        logMessage(std::string("Exception: ") + e.what(), "ERROR");
        _statusMessage = std::string("Error: ") + e.what();
        teardownNet();
        return false;
    }
}

} } // namespace client::ui
