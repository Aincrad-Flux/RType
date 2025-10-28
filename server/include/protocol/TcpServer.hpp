#pragma once
#include <asio.hpp>
#include <unordered_set>
#include <memory>
#include "common/Protocol.hpp"

namespace rtype::server {

class TcpServer : public std::enable_shared_from_this<TcpServer> {
public:
    TcpServer(asio::io_context& io, unsigned short tcpPort);

    void start();
    void stop();
    void broadcastStartGame(); //start when >= 2 players

private:
    using SocketPtr = std::shared_ptr<asio::ip::tcp::socket>;

    void doAccept();
    void startSession(SocketPtr sock);
    static void sendHeader(SocketPtr sock, rtype::net::MsgType t);

private:
    asio::ip::tcp::acceptor acceptor_;
    std::unordered_set<SocketPtr> clients_;
    bool running_{false};
};

}

