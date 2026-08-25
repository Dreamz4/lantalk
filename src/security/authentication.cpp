// LanTalk - Authenticator
// Challenge-response authentication using HMAC-SHA256

#include "authentication.hpp"
#include "crypto.hpp"
#include "logging/logger.hpp"
#include <iostream>

namespace lantalk {

// ---- Simple JSON helpers (no external library) ----------------------------

static std::string buildJson(
    const std::string& key1, const std::string& val1,
    const std::string& key2 = "", const std::string& val2 = "",
    const std::string& key3 = "", const std::string& val3 = "")
{
    std::string j = "{\"" + key1 + "\":\"" + val1 + "\"";
    if (!key2.empty()) j += ",\"" + key2 + "\":\"" + val2 + "\"";
    if (!key3.empty()) j += ",\"" + key3 + "\":\"" + val3 + "\"";
    j += "}";
    return j;
}

static std::string extractStr(const std::string& json, const std::string& key) {
    const std::string search = "\"" + key + "\":\"";
    size_t pos = json.find(search);
    if (pos == std::string::npos) return {};
    pos += search.size();
    std::string result;
    while (pos < json.size() && json[pos] != '"') {
        if (json[pos] == '\\' && pos + 1 < json.size()) {
            ++pos;
        }
        result += json[pos++];
    }
    return result;
}

// ---- Authenticator ---------------------------------------------------------

Authenticator::Authenticator(const AppConfig& config) : config_(config) {}

void Authenticator::sendChallenge(Connection& conn) {
    const std::string challenge = Crypto::generateChallenge();
    const std::string connAddr  = conn.remoteAddress();

    storePendingChallenge(connAddr, challenge);

    const std::string payload = buildJson("challenge", challenge);
    conn.send(MessageType::AUTH_REQUEST, payload);

    LOG_DEBUG("Sent AUTH_REQUEST challenge to " + connAddr);
}

bool Authenticator::verifyResponse(const std::string& challenge,
                                   const std::string& responsePayload) {
    if (config_.pskHash.empty()) {
        return true; // No PSK configured — always pass
    }

    const std::string response = extractStr(responsePayload, "response");
    if (response.empty()) {
        LOG_WARN("AUTH_RESPONSE has no 'response' field");
        return false;
    }

    const std::string expected = computeExpectedResponse(challenge);
    const bool ok = Crypto::secureCompare(response, expected);

    if (!ok) {
        LOG_WARN("AUTH_RESPONSE mismatch");
    }
    return ok;
}

std::string Authenticator::buildResponse(const std::string& challengePayload) {
    const std::string challenge = extractStr(challengePayload, "challenge");
    if (challenge.empty()) {
        LOG_WARN("AUTH_REQUEST has no 'challenge' field");
        return {};
    }

    const std::string response = computeExpectedResponse(challenge);
    return buildJson("response", response, "device_id", config_.deviceId);
}

void Authenticator::storePendingChallenge(const std::string& connId,
                                          const std::string& challenge) {
    std::lock_guard<std::mutex> lock(challengeMutex_);
    pendingChallenges_[connId] = challenge;
}

std::optional<std::string> Authenticator::getPendingChallenge(
    const std::string& connId) {
    std::lock_guard<std::mutex> lock(challengeMutex_);
    auto it = pendingChallenges_.find(connId);
    if (it != pendingChallenges_.end()) return it->second;
    return std::nullopt;
}

void Authenticator::clearPendingChallenge(const std::string& connId) {
    std::lock_guard<std::mutex> lock(challengeMutex_);
    pendingChallenges_.erase(connId);
}

std::string Authenticator::computeExpectedResponse(const std::string& challenge) const {
    if (config_.pskHash.empty()) return {};
    return Crypto::hmacSha256(config_.pskHash, challenge);
}

} // namespace lantalk
