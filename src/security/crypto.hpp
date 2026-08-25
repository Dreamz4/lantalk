#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include <filesystem>
#include <utility>

namespace lantalk {

class Crypto {
public:
    // Generate cryptographically secure random bytes
    static std::vector<uint8_t> randomBytes(size_t count);

    // Generate UUID v4 using random bytes
    static std::string generateUUID();

    // SHA-256 hash of input, returns hex string
    static std::string sha256(const std::string& input);
    static std::string sha256(const std::vector<uint8_t>& input);

    // HMAC-SHA256 of message with key, returns hex string
    static std::string hmacSha256(const std::string& key, const std::string& message);

    // Generate a challenge nonce (32 bytes, base64-encoded)
    static std::string generateChallenge();

    // Base64 encode/decode
    static std::string base64Encode(const std::vector<uint8_t>& data);
    static std::vector<uint8_t> base64Decode(const std::string& encoded);

    // Constant-time string comparison (prevents timing attacks)
    static bool secureCompare(const std::string& a, const std::string& b);

    // Generate self-signed TLS certificate + private key
    // Stores to certFile and keyFile paths
    // Returns true on success
    static bool generateSelfSignedCert(
        const std::filesystem::path& certFile,
        const std::filesystem::path& keyFile,
        const std::string& commonName
    );

    // Load or generate TLS credentials for this device
    // Returns {certPath, keyPath}
    static std::pair<std::filesystem::path, std::filesystem::path>
        getOrCreateTlsCredentials(const std::filesystem::path& configDir,
                                   const std::string& deviceId);
};

} // namespace lantalk
