#pragma once
#include <cstdint>
#include <string>
#include <unordered_map>
#include <optional>
#include <mutex>
#include <random>

namespace rtype::server::network {

class AuthStore {
public:
    AuthStore() : rng_(std::random_device{}()) {}

    // single-use player token
    std::uint32_t issueToken(const std::string& name) {
        std::lock_guard<std::mutex> lock(mutex_);
        std::uint32_t token;
        do {
            token = dist_(rng_);
        } while (token == 0 || tokens_.count(token) > 0);
        tokens_[token] = name;
        return token;
    }

    // Consume and erase a token; returns associated username if valid
    std::optional<std::string> consumeToken(std::uint32_t token) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = tokens_.find(token);
        if (it == tokens_.end()) return std::nullopt;
        std::string name = it->second;
        tokens_.erase(it);
        return name;
    }

private:
    std::mutex mutex_;
    std::unordered_map<std::uint32_t, std::string> tokens_;
    std::mt19937 rng_;
    std::uniform_int_distribution<std::uint32_t> dist_;
};

} // namespace rtype::server::network

