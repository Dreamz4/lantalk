#include "tcp_server.hpp"
#include <iostream>

#ifdef _WIN32
#else
#include <arpa/inet.h>
#endif

namespace lantalk {

TcpServer::TcpServer(uint16_t port, AcceptCallback onAccept)
    : port_(port), onAccept_(std::move(onAccept)) {}

TcpServer::~TcpServer() {
    stop();
}

bool TcpServer::start() {
    if (running_.load()) return true;

    SocketHandle h = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (h == kInvalidSocket) return false;

    listenSocket_ = TcpSocket(h);

    int opt = 1;
    ::setsockopt(h, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&opt), sizeof(opt));
#if defined(__linux__)
    ::setsockopt(h, SOL_SOCKET, SO_REUSEPORT, reinterpret_cast<const char*>(&opt), sizeof(opt));
#endif

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port_);

    if (::bind(h, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0) {
        listenSocket_.close();
        return false;
    }

    if (::listen(h, 16) < 0) {
        listenSocket_.close();
        return false;
    }

    running_.store(true);
    acceptThread_ = std::thread(&TcpServer::acceptLoop, this);
    return true;
}

void TcpServer::stop() {
    if (running_.exchange(false)) {
        listenSocket_.close();
        if (acceptThread_.joinable()) {
            acceptThread_.join();
        }
    }
}

std::string TcpServer::boundAddress() const {
    return listenSocket_.localAddress();
}

void TcpServer::acceptLoop() {
    while (running_.load()) {
        struct sockaddr_in clientAddr;
        socklen_t clientLen = sizeof(clientAddr);
        SocketHandle clientSocket = ::accept(listenSocket_.handle(), reinterpret_cast<struct sockaddr*>(&clientAddr), &clientLen);

        if (clientSocket == kInvalidSocket) {
#ifdef _WIN32
            int err = WSAGetLastError();
            if (err == WSAEINTR) continue;
#else
            if (errno == EINTR) continue;
#endif
            if (running_.load()) {
                // Not closing, real error
                std::cerr << "Accept error: " << lastSocketError() << "\n";
            }
            break;
        }

        if (running_.load() && onAccept_) {
            auto client = std::make_unique<TcpSocket>(clientSocket);
            std::thread([this, c = std::move(client)]() mutable {
                onAccept_(std::move(c));
            }).detach();
        } else {
#ifdef _WIN32
            ::closesocket(clientSocket);
#else
            ::close(clientSocket);
#endif
        }
    }
}

} // namespace lantalk
