#pragma once
#include "message.hpp"
#include "message_queue.hpp"
#include "../network/connection.hpp"
#include "../config/config.hpp"
#include <functional>
#include <memory>
#include <vector>
#include <mutex>
#include <atomic>
#include <thread>
#include <map>

namespace lantalk {

struct PeerInfo {
    std::string deviceId;
    std::string displayName;
    std::string address;
    uint16_t port{};
    std::string platform;
    ConnectionState state{ConnectionState::CONNECTED};
};

class ChatSession {
public:
    using MessageCallback = std::function<void(const ChatMessage&, const std::string& senderName)>;
    using PeerEventCallback = std::function<void(const PeerInfo&, bool connected)>;
    using AuthCallback = std::function<bool(const std::string& peerId, const std::string& challenge)>;

    explicit ChatSession(const AppConfig& config);
    ~ChatSession();

    // Set callbacks before start
    void setMessageCallback(MessageCallback cb);
    void setPeerEventCallback(PeerEventCallback cb);

    // Add a connection to this session
    void addConnection(std::unique_ptr<Connection> conn);

    // Send message to a specific peer (by device ID)
    bool sendTo(const std::string& peerId, const std::string& text);

    // Broadcast to all authenticated peers
    void broadcast(const std::string& text);

    // Get list of connected peers
    std::vector<PeerInfo> peers() const;

    // Get peer count
    size_t peerCount() const;

    // Handle incoming frame (called by Connection callback)
    void onFrame(const std::string& connId, Frame frame);
    void onDisconnect(const std::string& connId, const std::string& reason);

    // Start/stop session
    void start();
    void stop();

    const AppConfig& config() const { return config_; }

private:
    const AppConfig& config_;
    MessageCallback messageCallback_;
    PeerEventCallback peerEventCallback_;

    mutable std::mutex connMutex_;
    std::map<std::string, std::shared_ptr<Connection>> connections_;
    std::map<std::string, PeerInfo> peers_;

    std::atomic<bool> running_{false};

    // Generate connection ID
    static std::string makeConnId();

    // Protocol handlers
    void handleHello(const std::string& connId, const std::string& payload);
    void handleChatMessage(const std::string& connId, const std::string& payload);
    void handlePing(const std::string& connId);
    void handleDisconnect(const std::string& connId, const std::string& payload);
    void handleAuthRequest(const std::string& connId, const std::string& payload);
    void handleAuthResponse(const std::string& connId, const std::string& payload);

    // Send HELLO to a new connection
    void sendHello(const std::string& connId);

    // Ping/keepalive thread
    std::thread pingThread_;
    void pingLoop();
};

} // namespace lantalk
