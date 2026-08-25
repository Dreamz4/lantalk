#pragma once
#include "tcp_socket.hpp"
#include <string>
#include <memory>
#include <chrono>
#include <optional>

namespace lantalk {

struct ConnectError : public std::runtime_error {
    explicit ConnectError(const std::string& msg) : std::runtime_error(msg) {}
};

class TcpClient {
public:
    TcpClient() = default;
    ~TcpClient() = default;

    TcpClient(const TcpClient&) = delete;
    TcpClient& operator=(const TcpClient&) = delete;

    // Connect to host:port with timeout
    // Returns connected socket or throws ConnectError
    std::unique_ptr<TcpSocket> connect(
        const std::string& host,
        uint16_t port,
        std::chrono::seconds timeout = std::chrono::seconds{10}
    );

    // Resolve hostname to IP (IPv4)
    static std::optional<std::string> resolveHostname(const std::string& hostname);
};

} // namespace lantalk
