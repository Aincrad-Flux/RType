#include "TcpServer.hpp"
#include <array>
#include <iostream>

using namespace rtype::server;

TcpServer::TcpServer(asio::io_context& io, unsigned short tcpPort)
: acceptor_(io, asio::ip::tcp::endpoint(asio::ip::tcp::v4(), tcpPort)) {}

void TcpServer::start() {
    running_ = true;
    std::cout << "[server] Listening TCP on " << acceptor_.local_endpoint().port() << "\n";
    doAccept();
}

void TcpServer::stop() {
    running_ = false;
    asio::error_code ec;
    acceptor_.close(ec);
    for (auto& c : clients_) {
        if (c && c->is_open()) c->close(ec);
    }
    clients_.clear();
}

void TcpServer::broadcastStartGame() {
    for (auto& c : clients_) {
        if (c && c->is_open())
            sendHeader(c, rtype::net::MsgType::StartGame);
    }
}

void TcpServer::doAccept() {
    auto sock = std::make_shared<asio::ip::tcp::socket>(acceptor_.get_executor());
    acceptor_.async_accept(*sock, [self = shared_from_this(), sock](std::error_code ec) {
        if (!ec && self->running_) {
            self->clients_.insert(sock);
            self->startSession(sock);
        }
        if (self->running_) self->doAccept();
    });
}

void TcpServer::startSession(SocketPtr sock) {
    sendHeader(sock, rtype::net::MsgType::TcpWelcome);

    // track closure
    sock->async_wait(asio::ip::tcp::socket::wait_error,
        [self = shared_from_this(), sock](std::error_code) {
            self->clients_.erase(sock);
        });
}

void TcpServer::sendHeader(SocketPtr sock, rtype::net::MsgType t) {
    rtype::net::Header hdr{0, t, rtype::net::ProtocolVersion};
    auto buf = std::make_shared<std::array<char, sizeof(hdr)>>();
    std::memcpy(buf->data(), &hdr, sizeof(hdr));

    asio::async_write(*sock, asio::buffer(*buf),
        [buf](std::error_code, std::size_t) {});
}
