#pragma once
#include "tcp_socket.hpp"
#include "protocol.hpp"
#include <functional>
#include <thread>
#include <atomic>
#include <mutex>
#include <string>
#include <memory>
#include <queue>
#include <condition_variable>

namespace lantalk {

enum class ConnectionState {
    CONNECTED,
    AUTHENTICATING,
    AUTHENTICATED,
    DISCONNECTING,
    DISCONNECTED
};

struct ConnectionInfo {
    std::string peerId;
    std::string peerName;
    std::string peerAddress;
    uint16_t peerPort{};
    std::string platform;
};

class Connection {
public:
    using FrameCallback = std::function<void(Frame)>;
    using DisconnectCallback = std::function<void(const std::string& reason)>;

    explicit Connection(
        std::unique_ptr<TcpSocket> socket,
        FrameCallback onFrame,
        DisconnectCallback onDisconnect,
        size_t maxPayloadSize = kMaxPayloadSize
    );
    ~Connection();

    Connection(const Connection&) = delete;
    Connection& operator=(const Connection&) = delete;

    // Start receiver thread
    void start();

    // Send a frame (thread-safe)
    bool send(const Frame& frame);
    bool send(MessageType type, const std::string& jsonPayload);

    // Graceful disconnect
    void disconnect(const std::string& reason = "");

    ConnectionState state() const;
    void setState(ConnectionState state);
    const ConnectionInfo& info() const { return info_; }
    ConnectionInfo& info() { return info_; }

    std::string remoteAddress() const;

private:
    std::unique_ptr<TcpSocket> socket_;
    FrameCallback onFrame_;
    DisconnectCallback onDisconnect_;
    FrameReader reader_;

    std::atomic<ConnectionState> state_{ConnectionState::CONNECTED};
    ConnectionInfo info_;

    std::thread recvThread_;
    std::atomic<bool> running_{false};

    mutable std::mutex sendMutex_;

    void recvLoop();
};

} // namespace lantalk
