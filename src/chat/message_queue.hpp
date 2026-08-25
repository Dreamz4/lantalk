#pragma once
#include "message.hpp"
#include <queue>
#include <mutex>
#include <condition_variable>
#include <optional>
#include <chrono>
#include <functional>

namespace lantalk {

// Thread-safe bounded queue for ChatMessages
class MessageQueue {
public:
    explicit MessageQueue(size_t maxSize = 1000);

    // Push a message (blocks if full, up to timeout)
    bool push(ChatMessage msg, std::chrono::milliseconds timeout = std::chrono::milliseconds{100});

    // Pop a message (blocks until available or timeout)
    std::optional<ChatMessage> pop(std::chrono::milliseconds timeout = std::chrono::milliseconds{500});

    // Try pop without blocking
    std::optional<ChatMessage> tryPop();

    size_t size() const;
    bool empty() const;
    void clear();

    // Drain all pending messages calling callback for each
    void drain(const std::function<void(ChatMessage)>& callback);

private:
    mutable std::mutex mutex_;
    std::condition_variable notEmpty_;
    std::condition_variable notFull_;
    std::queue<ChatMessage> queue_;
    size_t maxSize_;
};

} // namespace lantalk
