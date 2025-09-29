#pragma once
#include <queue>
#include <mutex>
#include <condition_variable>
#include <optional>

namespace rtype::common {

template <typename T>
class EventQueue {
  public:
    void push(T value) {
        {
            std::lock_guard lock(mutex_);
            queue_.push(std::move(value));
        }
        cv_.notify_one();
    }

    T pop() {
        std::unique_lock lock(mutex_);
        cv_.wait(lock, [this]{ return !queue_.empty(); });
        T value = std::move(queue_.front());
        queue_.pop();
        return value;
    }

    std::optional<T> try_pop() {
        std::lock_guard lock(mutex_);
        if (queue_.empty()) return std::nullopt;
        T value = std::move(queue_.front());
        queue_.pop();
        return value;
    }

  private:
    std::queue<T> queue_;
    std::mutex mutex_;
    std::condition_variable cv_;
};

}
 