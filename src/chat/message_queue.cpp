#include "message_queue.hpp"

namespace lantalk {

MessageQueue::MessageQueue(size_t maxSize) : maxSize_(maxSize) {}

bool MessageQueue::push(ChatMessage msg, std::chrono::milliseconds timeout) {
    std::unique_lock<std::mutex> lock(mutex_);
    if (!notFull_.wait_for(lock, timeout, [this] { return queue_.size() < maxSize_; })) {
        return false;
    }
    queue_.push(std::move(msg));
    notEmpty_.notify_one();
    return true;
}

std::optional<ChatMessage> MessageQueue::pop(std::chrono::milliseconds timeout) {
    std::unique_lock<std::mutex> lock(mutex_);
    if (!notEmpty_.wait_for(lock, timeout, [this] { return !queue_.empty(); })) {
        return std::nullopt;
    }
    ChatMessage msg = std::move(queue_.front());
    queue_.pop();
    notFull_.notify_one();
    return msg;
}

std::optional<ChatMessage> MessageQueue::tryPop() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (queue_.empty()) return std::nullopt;
    ChatMessage msg = std::move(queue_.front());
    queue_.pop();
    notFull_.notify_one();
    return msg;
}

size_t MessageQueue::size() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return queue_.size();
}

bool MessageQueue::empty() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return queue_.empty();
}

void MessageQueue::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    std::queue<ChatMessage> emptyQueue;
    std::swap(queue_, emptyQueue);
    notFull_.notify_all();
}

void MessageQueue::drain(const std::function<void(ChatMessage)>& callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    while (!queue_.empty()) {
        callback(std::move(queue_.front()));
        queue_.pop();
    }
    notFull_.notify_all();
}

} // namespace lantalk
