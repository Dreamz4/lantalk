#include "lan_discovery.hpp"
#include <iostream>
#include <cstring>
#include <algorithm>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#if defined(_MSC_VER)
  #pragma comment(lib, "ws2_32.lib")
#endif
using socket_t = SOCKET;
#define ISVALIDSOCKET(s) ((s) != INVALID_SOCKET)
#define CLOSESOCKET(s) closesocket(s)
#define GETSOCKETERRNO() (WSAGetLastError())
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
using socket_t = int;
#define ISVALIDSOCKET(s) ((s) >= 0)
#define CLOSESOCKET(s) close(s)
#define GETSOCKETERRNO() (errno)
#endif

namespace lantalk {

LanDiscovery::LanDiscovery(const AppConfig& config) : config_(config) {
#ifdef _WIN32
    WSADATA wsa;
    WSAStartup(MAKEWORD(2,2), &wsa);
#endif
}

LanDiscovery::~LanDiscovery() {
    stopAnnounce();
#ifdef _WIN32
    WSACleanup();
#endif
}

bool LanDiscovery::startAnnounce() {
    if (running_.exchange(true)) return true;
    announceThread_ = std::thread(&LanDiscovery::announceLoop, this);
    return true;
}

void LanDiscovery::stopAnnounce() {
    if (!running_.exchange(false)) return;
    if (announceThread_.joinable()) {
        announceThread_.join();
    }
}

std::vector<uint8_t> LanDiscovery::buildAnnouncePacket() const {
    std::vector<uint8_t> pkt;
    
    // magic
    uint32_t magic = htonl(kDiscoveryMagic);
    pkt.insert(pkt.end(), (uint8_t*)&magic, (uint8_t*)&magic + 4);
    
    // version
    pkt.push_back(1);
    
    // tcp_port
    uint16_t port = htons(config_.listenPort);
    pkt.insert(pkt.end(), (uint8_t*)&port, (uint8_t*)&port + 2);
    
    // device_id (36 bytes)
    std::string did = config_.deviceId;
    did.resize(36, '\0'); // pad or truncate
    pkt.insert(pkt.end(), did.begin(), did.begin() + 36);
    
    // name
    std::string name = config_.displayName;
    if (name.length() > 255) name.resize(255);
    pkt.push_back(static_cast<uint8_t>(name.length()));
    pkt.insert(pkt.end(), name.begin(), name.end());
    
    // platform
    std::string plat = "linux"; // simple default
#ifdef _WIN32
    plat = "windows";
#endif
    if (plat.length() > 255) plat.resize(255);
    pkt.push_back(static_cast<uint8_t>(plat.length()));
    pkt.insert(pkt.end(), plat.begin(), plat.end());
    
    return pkt;
}

std::optional<DiscoveredPeer> LanDiscovery::parseAnnouncePacket(
    const uint8_t* data, size_t len, const std::string& senderIp) 
{
    if (len < 4 + 1 + 2 + 36 + 1) return std::nullopt;
    
    size_t offset = 0;
    
    uint32_t magic;
    std::memcpy(&magic, data + offset, 4);
    if (ntohl(magic) != kDiscoveryMagic) return std::nullopt;
    offset += 4;
    
    uint8_t version = data[offset++];
    
    uint16_t port;
    std::memcpy(&port, data + offset, 2);
    port = ntohs(port);
    offset += 2;
    
    std::string deviceId((const char*)data + offset, 36);
    // trim nulls if any
    deviceId.erase(std::find(deviceId.begin(), deviceId.end(), '\0'), deviceId.end());
    offset += 36;
    
    if (offset >= len) return std::nullopt;
    uint8_t nameLen = data[offset++];
    if (offset + nameLen > len) return std::nullopt;
    std::string name((const char*)data + offset, nameLen);
    offset += nameLen;
    
    std::string platform;
    if (offset < len) {
        uint8_t platLen = data[offset++];
        if (offset + platLen <= len) {
            platform = std::string((const char*)data + offset, platLen);
        }
    }
    
    DiscoveredPeer peer;
    peer.ipAddress = senderIp;
    peer.tcpPort = port;
    peer.deviceId = deviceId;
    peer.displayName = name;
    peer.platform = platform;
    peer.protocolVersion = version;
    peer.lastSeen = std::chrono::steady_clock::now();
    return peer;
}

void LanDiscovery::announce() {
    socket_t s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (!ISVALIDSOCKET(s)) return;
    
    int broadcast = 1;
    setsockopt(s, SOL_SOCKET, SO_BROADCAST, (char*)&broadcast, sizeof(broadcast));
    
    struct sockaddr_in baddr;
    std::memset(&baddr, 0, sizeof(baddr));
    baddr.sin_family = AF_INET;
    baddr.sin_port = htons(kDiscoveryPort);
    baddr.sin_addr.s_addr = INADDR_BROADCAST;
    
    auto pkt = buildAnnouncePacket();
    sendto(s, (const char*)pkt.data(), pkt.size(), 0, (struct sockaddr*)&baddr, sizeof(baddr));
    
    CLOSESOCKET(s);
}

void LanDiscovery::announceLoop() {
    while (running_) {
        announce();
        
        // Sleep for 30s but check running_ periodically
        for (int i = 0; i < 300 && running_; ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
}

std::vector<DiscoveredPeer> LanDiscovery::scan(PeerFoundCallback onFound) {
    std::vector<DiscoveredPeer> results;
    
    socket_t s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (!ISVALIDSOCKET(s)) return results;
    
    int reuse = 1;
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (char*)&reuse, sizeof(reuse));
    
    struct sockaddr_in laddr;
    std::memset(&laddr, 0, sizeof(laddr));
    laddr.sin_family = AF_INET;
    laddr.sin_port = htons(kDiscoveryPort);
    laddr.sin_addr.s_addr = INADDR_ANY;
    
    if (bind(s, (struct sockaddr*)&laddr, sizeof(laddr)) < 0) {
        CLOSESOCKET(s);
        return results;
    }
    
    // Set non-blocking
#ifdef _WIN32
    u_long mode = 1;
    ioctlsocket(s, FIONBIO, &mode);
#else
    int flags = fcntl(s, F_GETFL, 0);
    fcntl(s, F_SETFL, flags | O_NONBLOCK);
#endif

    auto endTime = std::chrono::steady_clock::now() + std::chrono::seconds(kScanTimeoutSecs);
    
    char buf[1024];
    while (std::chrono::steady_clock::now() < endTime) {
        struct sockaddr_in raddr;
        socklen_t raddr_len = sizeof(raddr);
        int n = recvfrom(s, buf, sizeof(buf), 0, (struct sockaddr*)&raddr, &raddr_len);
        
        if (n > 0) {
            char ipStr[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &(raddr.sin_addr), ipStr, INET_ADDRSTRLEN);
            
            auto peerOpt = parseAnnouncePacket((uint8_t*)buf, n, ipStr);
            if (peerOpt && peerOpt->deviceId != config_.deviceId) {
                // Check if already found
                bool found = false;
                for (auto& p : results) {
                    if (p.deviceId == peerOpt->deviceId) {
                        p = *peerOpt;
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    results.push_back(*peerOpt);
                    if (onFound) onFound(*peerOpt);
                }
            }
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
    }
    
    CLOSESOCKET(s);
    return results;
}

} // namespace lantalk
