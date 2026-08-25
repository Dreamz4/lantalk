#pragma once
#include <string>
#include <cstdint>
#include <filesystem>
#include <optional>

namespace lantalk {

struct AppConfig {
    // Identity
    std::string deviceId;       // UUID v4, generated once
    std::string displayName;    // e.g. "Abhishek"
    std::string platform;       // "Linux", "Windows", "macOS"

    // Network
    uint16_t listenPort{5050};
    uint16_t discoveryPort{5051};
    bool enableIPv6{false};
    uint32_t maxMessageSize{65536};
    int connectTimeoutSecs{10};
    int readTimeoutSecs{30};
    int keepaliveIntervalSecs{15};

    // Discovery
    bool discoveryEnabled{true};

    // Security
    std::string pskHash;        // bcrypt/SHA-256 of shared passphrase
    bool requireAuth{false};    // if true, require PSK match

    // Logging
    std::string logLevel{"INFO"};
    std::string logFile;
};

class Config {
public:
    explicit Config();

    // Returns platform config directory: ~/.config/lantalk, %APPDATA%/LanTalk, etc.
    static std::filesystem::path configDirectory();
    static std::filesystem::path configFilePath();

    bool load();
    bool save() const;

    AppConfig& get() { return config_; }
    const AppConfig& get() const { return config_; }

    // Set a named key, returns false if key unknown
    bool set(const std::string& key, const std::string& value);
    std::optional<std::string> getAsString(const std::string& key) const;

    void print() const;

    // Generate a new UUID v4 using OpenSSL RAND_bytes
    static std::string generateUUID();
    // Get or create platform string
    static std::string platformName();

private:
    AppConfig config_;
    void ensureDeviceId();
    void ensureDisplayName();
    static std::string readHostname();
};

} // namespace lantalk
