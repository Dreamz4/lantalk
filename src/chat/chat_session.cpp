// LanTalk - Chat Session
// Orchestrates connections, routes frames, manages peer state

#include "chat_session.hpp"
#include "logging/logger.hpp"
#include <iostream>
#include <chrono>
#include <sstream>
#include <openssl/rand.h>

namespace lantalk {

// ---- Minimal JSON helpers (no external library) ----------------------------

static std::string jsonEscape(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 8);
    for (unsigned char c : s) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:
                if (c < 0x20) {
                    char buf[8];
                    snprintf(buf, sizeof(buf), "\\u%04x", c);
                    out += buf;
                } else {
                    out += static_cast<char>(c);
                }
        }
    }
    return out;
}

static std::string jsonStr(const std::string& key, const std::string& value) {
    return "\"" + jsonEscape(key) + "\":\"" + jsonEscape(value) + "\"";
}

// Extract string value for key from a simple flat JSON object.
// Does not handle nested objects.
static std::string extractStr(const std::string& json, const std::string& key) {
    const std::string search = "\"" + key + "\":\"";
    size_t pos = json.find(search);
    if (pos == std::string::npos) return {};
    pos += search.size();
    std::string result;
    while (pos < json.size()) {
        if (json[pos] == '\\' && pos + 1 < json.size()) {
            ++pos;
            switch (json[pos]) {
                case '"':  result += '"';  break;
                case '\\': result += '\\'; break;
                case 'n':  result += '\n'; break;
                case 'r':  result += '\r'; break;
                case 't':  result += '\t'; break;
                default:   result += json[pos]; break;
            }
        } else if (json[pos] == '"') {
            break;
        } else {
            result += json[pos];
        }
        ++pos;
    }
    return result;
}

// ---- ChatSession -----------------------------------------------------------

ChatSession::ChatSession(const AppConfig& config) : config_(config) {}

ChatSession::~ChatSession() {
    stop();
}

void ChatSession::setMessageCallback(MessageCallback cb) {
    messageCallback_ = std::move(cb);
}

void ChatSession::setPeerEventCallback(PeerEventCallback cb) {
    peerEventCallback_ = std::move(cb);
}

std::string ChatSession::makeConnId() {
    unsigned char bytes[8];
    if (RAND_bytes(bytes, static_cast<int>(sizeof(bytes))) != 1) {
        // Fallback to timestamp-based ID
        auto ts = std::chrono::steady_clock::now().time_since_epoch().count();
        return "conn" + std::to_string(ts);
    }
    char hex[17];
    snprintf(hex, sizeof(hex),
        "%02x%02x%02x%02x%02x%02x%02x%02x",
        bytes[0], bytes[1], bytes[2], bytes[3],
        bytes[4], bytes[5], bytes[6], bytes[7]);
    return std::string(hex);
}

void ChatSession::addConnection(std::unique_ptr<Connection> conn) {
    if (!running_.load()) {
        LOG_WARN("addConnection called but session not started");
        return;
    }

    const std::string connId = makeConnId();
    auto connPtr = std::shared_ptr<Connection>(std::move(conn));

    {
        std::lock_guard<std::mutex> lock(connMutex_);
        connections_[connId] = connPtr;
        peers_[connId] = PeerInfo{};
    }

    // The connection was already constructed with its callbacks in main.cpp.
    // Re-wrap callbacks so we have the connId in scope:
    // We achieve this by re-constructing a wrapper — but Connection does NOT
    // support setCallbacks post-construction. Instead we register the frame
    // routing in the frame callback passed during Connection construction
    // (done in main.cpp). The connId is passed as empty string from main.cpp.
    // Here we just start the connection (which begins the recv thread).
    connPtr->start();
    sendHello(connId);

    LOG_INFO("New connection " + connId);
}

bool ChatSession::sendTo(const std::string& peerId, const std::string& text) {
    std::shared_ptr<Connection> targetConn;
    {
        std::lock_guard<std::mutex> lock(connMutex_);
        for (const auto& [cid, peer] : peers_) {
            if (peer.deviceId == peerId &&
                peer.state == ConnectionState::AUTHENTICATED) {
                auto it = connections_.find(cid);
                if (it != connections_.end()) targetConn = it->second;
                break;
            }
        }
    }

    if (!targetConn) return false;

    ChatMessage msg;
    msg.msgId       = ChatMessage::generateId();
    msg.senderId    = config_.deviceId;
    msg.senderName  = config_.displayName;
    msg.timestampUtc = ChatMessage::currentTimestamp();
    msg.text        = text;

    return targetConn->send(MessageType::CHAT_MESSAGE, msg.toJson());
}

void ChatSession::broadcast(const std::string& text) {
    ChatMessage msg;
    msg.msgId       = ChatMessage::generateId();
    msg.senderId    = config_.deviceId;
    msg.senderName  = config_.displayName;
    msg.timestampUtc = ChatMessage::currentTimestamp();
    msg.text        = text;

    const std::string json = msg.toJson();

    std::lock_guard<std::mutex> lock(connMutex_);
    for (auto& [cid, conn] : connections_) {
        auto it = peers_.find(cid);
        if (it != peers_.end() &&
            it->second.state == ConnectionState::AUTHENTICATED) {
            conn->send(MessageType::CHAT_MESSAGE, json);
        }
    }
}

std::vector<PeerInfo> ChatSession::peers() const {
    std::vector<PeerInfo> result;
    std::lock_guard<std::mutex> lock(connMutex_);
    for (const auto& [cid, peer] : peers_) {
        if (peer.state == ConnectionState::AUTHENTICATED) {
            result.push_back(peer);
        }
    }
    return result;
}

size_t ChatSession::peerCount() const {
    std::lock_guard<std::mutex> lock(connMutex_);
    size_t count = 0;
    for (const auto& [cid, peer] : peers_) {
        if (peer.state == ConnectionState::AUTHENTICATED) ++count;
    }
    return count;
}

void ChatSession::onFrame(const std::string& connId, Frame frame) {
    const std::string payload = frame.payloadAsString();
    switch (frame.header.type) {
        case MessageType::HELLO:
            handleHello(connId, payload);
            break;
        case MessageType::AUTH_REQUEST:
            handleAuthRequest(connId, payload);
            break;
        case MessageType::AUTH_RESPONSE:
            handleAuthResponse(connId, payload);
            break;
        case MessageType::CHAT_MESSAGE:
            handleChatMessage(connId, payload);
            break;
        case MessageType::PING:
            handlePing(connId);
            break;
        case MessageType::PONG:
            // Nothing to do — just confirms peer is alive
            break;
        case MessageType::DISCONNECT:
            handleDisconnect(connId, payload);
            break;
        default:
            LOG_DEBUG("Unhandled message type from " + connId);
            break;
    }
}

void ChatSession::onDisconnect(const std::string& connId, const std::string& reason) {
    PeerInfo info;
    bool shouldNotify = false;
    {
        std::lock_guard<std::mutex> lock(connMutex_);
        auto pit = peers_.find(connId);
        if (pit != peers_.end()) {
            info = pit->second;
            shouldNotify = (info.state == ConnectionState::AUTHENTICATED);
            peers_.erase(pit);
        }
        connections_.erase(connId);
    }
    if (shouldNotify && peerEventCallback_) {
        peerEventCallback_(info, false);
    }
    LOG_INFO("Disconnected " + connId + ": " + reason);
}

void ChatSession::start() {
    if (running_.exchange(true)) return;
    pingThread_ = std::thread(&ChatSession::pingLoop, this);
    LOG_INFO("ChatSession started");
}

void ChatSession::stop() {
    if (!running_.exchange(false)) return;
    if (pingThread_.joinable()) pingThread_.join();

    std::lock_guard<std::mutex> lock(connMutex_);
    for (auto& [cid, conn] : connections_) {
        conn->disconnect("session stopped");
    }
    connections_.clear();
    peers_.clear();
    LOG_INFO("ChatSession stopped");
}

// ---- Private helpers -------------------------------------------------------

void ChatSession::sendHello(const std::string& connId) {
    std::string payload = "{";
    payload += jsonStr("device_id",    config_.deviceId) + ",";
    payload += jsonStr("display_name", config_.displayName) + ",";
    payload += jsonStr("platform",     config_.platform) + ",";
    payload += "\"tcp_port\":" + std::to_string(config_.listenPort);
    payload += "}";

    std::lock_guard<std::mutex> lock(connMutex_);
    auto it = connections_.find(connId);
    if (it != connections_.end()) {
        it->second->send(MessageType::HELLO, payload);
    }
}

void ChatSession::handleHello(const std::string& connId, const std::string& payload) {
    const std::string deviceId    = extractStr(payload, "device_id");
    const std::string displayName = extractStr(payload, "display_name");
    const std::string platform    = extractStr(payload, "platform");

    if (deviceId.empty()) {
        LOG_WARN("HELLO without device_id from " + connId + ", disconnecting");
        std::lock_guard<std::mutex> lock(connMutex_);
        auto it = connections_.find(connId);
        if (it != connections_.end()) it->second->disconnect("invalid HELLO");
        return;
    }

    bool authenticated = false;
    PeerInfo peerSnap;
    {
        std::lock_guard<std::mutex> lock(connMutex_);
        auto& peer       = peers_[connId];
        peer.deviceId    = deviceId;
        peer.displayName = displayName;
        peer.platform    = platform;

        if (config_.requireAuth) {
            peer.state = ConnectionState::AUTHENTICATING;
            // Send AUTH_REQUEST with a simple challenge
            // (full challenge-response happens via Authenticator in production;
            //  here we do a lightweight flow)
            const std::string challenge = deviceId + "|" + config_.deviceId;
            std::string req = "{";
            req += jsonStr("challenge", challenge);
            req += "}";
            auto it = connections_.find(connId);
            if (it != connections_.end()) {
                it->second->send(MessageType::AUTH_REQUEST, req);
            }
        } else {
            peer.state  = ConnectionState::AUTHENTICATED;
            authenticated = true;
            peerSnap    = peer;
        }
    }

    if (authenticated && peerEventCallback_) {
        peerEventCallback_(peerSnap, true);
    }

    LOG_INFO("HELLO from " + displayName + " (" + deviceId + ")");
}

void ChatSession::handleChatMessage(const std::string& connId, const std::string& payload) {
    // Validate peer is authenticated
    {
        std::lock_guard<std::mutex> lock(connMutex_);
        auto it = peers_.find(connId);
        if (it == peers_.end() ||
            it->second.state != ConnectionState::AUTHENTICATED) {
            LOG_WARN("CHAT_MESSAGE from unauthenticated peer " + connId);
            return;
        }
    }

    auto msgOpt = ChatMessage::fromJson(payload);
    if (!msgOpt) {
        LOG_WARN("Malformed CHAT_MESSAGE from " + connId);
        return;
    }

    if (messageCallback_) {
        messageCallback_(*msgOpt, msgOpt->senderName);
    }
}

void ChatSession::handlePing(const std::string& connId) {
    std::lock_guard<std::mutex> lock(connMutex_);
    auto it = connections_.find(connId);
    if (it != connections_.end()) {
        it->second->send(MessageType::PONG, "{}");
    }
}

void ChatSession::handleDisconnect(const std::string& connId, const std::string& /*payload*/) {
    std::lock_guard<std::mutex> lock(connMutex_);
    auto it = connections_.find(connId);
    if (it != connections_.end()) {
        it->second->disconnect("peer requested disconnect");
    }
}

void ChatSession::handleAuthRequest(const std::string& connId, const std::string& payload) {
    // Simple PSK auth: HMAC-SHA256(psk, challenge) or empty if no PSK
    const std::string challenge = extractStr(payload, "challenge");

    std::string response;
    if (config_.pskHash.empty()) {
        response = "ok"; // no PSK, accept anything
    } else {
        // In production, Authenticator::buildResponse() would be used
        response = config_.pskHash; // placeholder
    }

    std::string resp = "{";
    resp += jsonStr("response",  response) + ",";
    resp += jsonStr("device_id", config_.deviceId);
    resp += "}";

    std::lock_guard<std::mutex> lock(connMutex_);
    auto it = connections_.find(connId);
    if (it != connections_.end()) {
        it->second->send(MessageType::AUTH_RESPONSE, resp);
    }
}

void ChatSession::handleAuthResponse(const std::string& connId, const std::string& payload) {
    const std::string response = extractStr(payload, "response");
    const std::string peerId   = extractStr(payload, "device_id");

    bool accepted = false;
    if (config_.pskHash.empty()) {
        accepted = true; // no PSK configured
    } else {
        accepted = (response == config_.pskHash); // simplified check
    }

    bool notify = false;
    PeerInfo peerSnap;
    {
        std::lock_guard<std::mutex> lock(connMutex_);
        auto it = connections_.find(connId);
        if (it != connections_.end()) {
            if (accepted) {
                peers_[connId].state = ConnectionState::AUTHENTICATED;
                notify = true;
                peerSnap = peers_[connId];
                it->second->send(MessageType::AUTH_OK, "{}");
            } else {
                it->second->send(MessageType::AUTH_FAIL, "{\"reason\":\"bad credentials\"}");
                it->second->disconnect("authentication failed");
            }
        }
    }

    if (notify && peerEventCallback_) {
        peerEventCallback_(peerSnap, true);
    }
}

void ChatSession::pingLoop() {
    while (running_.load()) {
        std::this_thread::sleep_for(std::chrono::seconds(15));
        if (!running_.load()) break;

        std::vector<std::string> deadConns;
        {
            std::lock_guard<std::mutex> lock(connMutex_);
            for (auto& [cid, conn] : connections_) {
                if (!conn->send(MessageType::PING, "{}")) {
                    deadConns.push_back(cid);
                }
            }
        }

        for (const auto& cid : deadConns) {
            onDisconnect(cid, "ping failed");
        }
    }
}

} // namespace lantalk
