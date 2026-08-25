#pragma once
#include "../network/connection.hpp"
#include "../config/config.hpp"
#include <string>
#include <functional>
#include <optional>
#include <mutex>
#include <map>

namespace lantalk {

class Authenticator {
public:
    using AuthResult = std::function<void(bool success, const std::string& peerId)>;

    explicit Authenticator(const AppConfig& config);

    // --- Server side ---
    // Send AUTH_REQUEST challenge to connection
    void sendChallenge(Connection& conn);

    // Verify AUTH_RESPONSE from peer
    // Returns true if valid
    bool verifyResponse(const std::string& challenge,
                        const std::string& responsePayload);

    // --- Client side ---
    // Generate AUTH_RESPONSE payload given challenge payload
    std::string buildResponse(const std::string& challengePayload);

    // Store the pending challenge for a connection
    void storePendingChallenge(const std::string& connId, const std::string& challenge);
    std::optional<std::string> getPendingChallenge(const std::string& connId);
    void clearPendingChallenge(const std::string& connId);

private:
    const AppConfig& config_;
    std::mutex challengeMutex_;
    std::map<std::string, std::string> pendingChallenges_;

    // Compute expected response: HMAC-SHA256(psk, challenge)
    std::string computeExpectedResponse(const std::string& challenge) const;
};

} // namespace lantalk
