#include "connection.hpp"
#include <iostream>

namespace lantalk {

Connection::Connection(
    std::unique_ptr<TcpSocket> socket,
    FrameCallback onFrame,
    DisconnectCallback onDisconnect,
    size_t maxPayloadSize
) : socket_(std::move(socket)),
    onFrame_(std::move(onFrame)),
    onDisconnect_(std::move(onDisconnect)),
    reader_(maxPayloadSize) {
}

Connection::~Connection() {
    running_.store(false);
    if (socket_) socket_->close();
    if (recvThread_.joinable()) {
        recvThread_.join();
    }
}

void Connection::start() {
    running_.store(true);
    recvThread_ = std::thread(&Connection::recvLoop, this);
}

bool Connection::send(const Frame& frame) {
    if (!running_.load() || !socket_) return false;
    std::vector<uint8_t> data = frame.serialize();
    std::lock_guard<std::mutex> lock(sendMutex_);
    return socket_->sendAll(data);
}

bool Connection::send(MessageType type, const std::string& jsonPayload) {
    return send(Frame::create(type, jsonPayload));
}

void Connection::disconnect(const std::string& reason) {
    if (state_.load() == ConnectionState::DISCONNECTED) return;
    setState(ConnectionState::DISCONNECTING);
    send(MessageType::DISCONNECT, reason);
    running_.store(false);
    if (socket_) socket_->close();
    setState(ConnectionState::DISCONNECTED);
}

ConnectionState Connection::state() const {
    return state_.load();
}

void Connection::setState(ConnectionState state) {
    state_.store(state);
}

std::string Connection::remoteAddress() const {
    if (socket_) return socket_->remoteAddress();
    return "";
}

void Connection::recvLoop() {
    std::vector<uint8_t> buf(4096);
    auto lastCheck = std::chrono::steady_clock::now();
    int frameCount = 0;

    while (running_.load()) {
        ssize_t bytes = socket_->recv(buf.data(), buf.size());
        if (bytes <= 0) {
            if (running_.load()) {
                running_.store(false);
                setState(ConnectionState::DISCONNECTED);
                if (onDisconnect_) {
                    onDisconnect_("Connection closed");
                }
            }
            break;
        }

        try {
            auto frames = reader_.feed(buf.data(), bytes);
            for (auto& f : frames) {
                frameCount++;
                if (onFrame_) {
                    onFrame_(std::move(f));
                }
            }

            auto now = std::chrono::steady_clock::now();
            if (std::chrono::duration_cast<std::chrono::seconds>(now - lastCheck).count() >= 1) {
                if (frameCount > 100) {
                    disconnect("Rate limit exceeded");
                    break;
                }
                frameCount = 0;
                lastCheck = now;
            }

        } catch (const std::exception& e) {
            disconnect(std::string("Protocol error: ") + e.what());
            break;
        }
    }
}

} // namespace lantalk
