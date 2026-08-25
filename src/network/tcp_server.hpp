#pragma once
#include "tcp_socket.hpp"
#include <functional>
#include <memory>
#include <thread>
#include <atomic>
#include <string>
#include <cstdint>

namespace lantalk {

// Callback invoked when a new client connects
// Takes ownership of the accepted TcpSocket
using AcceptCallback = std::function<void(std::unique_ptr<TcpSocket>)>;

class TcpServer {
public:
    explicit TcpServer(uint16_t port, AcceptCallback onAccept);
    ~TcpServer();

    // Non-copyable, non-movable
    TcpServer(const TcpServer&) = delete;
    TcpServer& operator=(const TcpServer&) = delete;

    // Start listening and accepting (blocks on accept in background thread)
    // Returns true if bind succeeded
    bool start();

    // Stop accepting new connections
    void stop();

    bool isRunning() const { return running_.load(); }
    uint16_t port() const { return port_; }
    std::string boundAddress() const;

private:
    uint16_t port_;
    AcceptCallback onAccept_;
    TcpSocket listenSocket_;
    std::atomic<bool> running_{false};
    std::thread acceptThread_;

    void acceptLoop();
};

} // namespace lantalk
