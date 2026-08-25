#pragma once
#include "../config/config.hpp"
#include <string>
#include <vector>
#include <functional>
#include <thread>
#include <atomic>
#include <mutex>
#include <chrono>
#include <optional>

namespace lantalk {

struct DiscoveredPeer {
    std::string ipAddress;
    uint16_t tcpPort{};
    std::string deviceId;
    std::string displayName;
    std::string platform;
    uint8_t protocolVersion{};
    std::chrono::steady_clock::time_point lastSeen;
};

using PeerFoundCallback = std::function<void(const DiscoveredPeer&)>;

class LanDiscovery {
public:
    static inline constexpr uint16_t kDiscoveryPort = 5051;
    static inline constexpr uint32_t kDiscoveryMagic = 0x4C544C44; // 'LTLD'
    static inline constexpr int kScanTimeoutSecs = 3;

    explicit LanDiscovery(const AppConfig& config);
    ~LanDiscovery();

    LanDiscovery(const LanDiscovery&) = delete;
    LanDiscovery& operator=(const LanDiscovery&) = delete;

    // Start background announce thread (broadcasts presence every 30s)
    bool startAnnounce();
    void stopAnnounce();

    // Scan for peers (blocks for kScanTimeoutSecs seconds)
    std::vector<DiscoveredPeer> scan(PeerFoundCallback onFound = nullptr);

    // Send a single announce
    void announce();

private:
    const AppConfig& config_;
    std::atomic<bool> running_{false};
    std::thread announceThread_;
    mutable std::mutex peersMutex_;
    std::vector<DiscoveredPeer> knownPeers_;

    // UDP discovery packet format (binary, fixed fields):
    // [magic:uint32][version:uint8][tcp_port:uint16]
    // [device_id:36 bytes ASCII UUID]
    // [name_len:uint8][display_name:N bytes]
    // [platform_len:uint8][platform:N bytes]
    // Total max: ~150 bytes

    std::vector<uint8_t> buildAnnouncePacket() const;
    static std::optional<DiscoveredPeer> parseAnnouncePacket(
        const uint8_t* data, size_t len, const std::string& senderIp);

    void announceLoop();
};

} // namespace lantalk
