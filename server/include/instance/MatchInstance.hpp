#pragma once
#include <memory>
#include <asio.hpp>
#include "gameplay/GameSession.hpp"

namespace rtype::server::instance {

class MatchInstance {
public:
    MatchInstance(asio::io_context& io, std::unique_ptr<rtype::server::gameplay::GameSession> session)
        : io_(io), session_(std::move(session)) {}

    void start() { if (session_) session_->start(); }
    void stop()  { if (session_) session_->stop(); }

    rtype::server::gameplay::GameSession& session() { return *session_; }

private:
    asio::io_context& io_;
    std::unique_ptr<rtype::server::gameplay::GameSession> session_;
};

}
