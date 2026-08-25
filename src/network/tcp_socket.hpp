#pragma once
#include "socket_types.hpp"
#include <string>
#include <vector>
#include <cstdint>
#include <chrono>
#include <stdexcept>

namespace lantalk {

// RAII wrapper around a TCP socket handle
class TcpSocket {
public:
    TcpSocket();
    explicit TcpSocket(SocketHandle handle);
    ~TcpSocket();

    // Non-copyable
    TcpSocket(const TcpSocket&) = delete;
    TcpSocket& operator=(const TcpSocket&) = delete;

    // Movable
    TcpSocket(TcpSocket&& other) noexcept;
    TcpSocket& operator=(TcpSocket&& other) noexcept;

    bool isValid() const;
    SocketHandle handle() const { return handle_; }

    // Set socket to blocking or non-blocking
    void setBlocking(bool blocking);

    // Set recv/send timeouts (milliseconds, 0 = infinite)
    void setReceiveTimeout(std::chrono::milliseconds timeout);
    void setSendTimeout(std::chrono::milliseconds timeout);

    // Enable TCP keepalive
    void setKeepAlive(bool enable);

    // Enable TCP_NODELAY
    void setNoDelay(bool enable);

    // Set socket receive buffer size
    void setReceiveBufferSize(int bytes);

    // Raw send - handles partial writes
    // Returns false if connection closed
    bool sendAll(const uint8_t* data, size_t len);
    bool sendAll(const std::vector<uint8_t>& data);

    // Raw recv - handles partial reads
    // Returns number of bytes read, 0 if connection closed, -1 on error
    ssize_t recv(uint8_t* buffer, size_t bufferSize);

    // Close the socket
    void close();

    // Get local endpoint
    std::string localAddress() const;
    uint16_t localPort() const;

    // Get remote endpoint
    std::string remoteAddress() const;
    uint16_t remotePort() const;

    // Underlying handle (for SSL wrapping)
    SocketHandle release();

private:
    SocketHandle handle_;
    void applySocketOption(int level, int option, int value);
};

// Socket error type
struct SocketError : public std::runtime_error {
    explicit SocketError(const std::string& msg) : std::runtime_error(msg) {}
};

} // namespace lantalk
