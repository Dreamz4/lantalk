#include "tcp_socket.hpp"
#include <cstring>
#include <iostream>

#ifdef _WIN32
  #include <winsock2.h>
  #include <ws2tcpip.h>
  #pragma comment(lib, "ws2_32.lib")
#else
  #include <sys/socket.h>
  #include <netinet/in.h>
  #include <netinet/tcp.h>
  #include <arpa/inet.h>
  #include <unistd.h>
  #include <fcntl.h>
  #include <errno.h>
#endif

namespace lantalk {

std::string lastSocketError() {
#ifdef _WIN32
    int err = WSAGetLastError();
    if (err == 0) return "No error";
    char* msgBuf = nullptr;
    size_t size = FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                                 NULL, err, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&msgBuf, 0, NULL);
    std::string message(msgBuf, size);
    LocalFree(msgBuf);
    return message;
#else
    return std::string(strerror(errno));
#endif
}

bool initializeSockets() {
#ifdef _WIN32
    WSADATA wsaData;
    return WSAStartup(MAKEWORD(2, 2), &wsaData) == 0;
#else
    return true;
#endif
}

void cleanupSockets() {
#ifdef _WIN32
    WSACleanup();
#endif
}

TcpSocket::TcpSocket() : handle_(kInvalidSocket) {}

TcpSocket::TcpSocket(SocketHandle handle) : handle_(handle) {}

TcpSocket::~TcpSocket() {
    close();
}

TcpSocket::TcpSocket(TcpSocket&& other) noexcept : handle_(other.handle_) {
    other.handle_ = kInvalidSocket;
}

TcpSocket& TcpSocket::operator=(TcpSocket&& other) noexcept {
    if (this != &other) {
        close();
        handle_ = other.handle_;
        other.handle_ = kInvalidSocket;
    }
    return *this;
}

bool TcpSocket::isValid() const {
    return handle_ != kInvalidSocket;
}

void TcpSocket::setBlocking(bool blocking) {
    if (!isValid()) return;
#ifdef _WIN32
    u_long mode = blocking ? 0 : 1;
    ioctlsocket(handle_, FIONBIO, &mode);
#else
    int flags = fcntl(handle_, F_GETFL, 0);
    if (flags == -1) return;
    if (blocking)
        fcntl(handle_, F_SETFL, flags & ~O_NONBLOCK);
    else
        fcntl(handle_, F_SETFL, flags | O_NONBLOCK);
#endif
}

void TcpSocket::applySocketOption(int level, int option, int value) {
    if (!isValid()) return;
    setsockopt(handle_, level, option, reinterpret_cast<const char*>(&value), sizeof(value));
}

void TcpSocket::setReceiveTimeout(std::chrono::milliseconds timeout) {
    if (!isValid()) return;
#ifdef _WIN32
    DWORD ms = static_cast<DWORD>(timeout.count());
    setsockopt(handle_, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&ms), sizeof(ms));
#else
    struct timeval tv;
    tv.tv_sec = timeout.count() / 1000;
    tv.tv_usec = (timeout.count() % 1000) * 1000;
    setsockopt(handle_, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&tv), sizeof(tv));
#endif
}

void TcpSocket::setSendTimeout(std::chrono::milliseconds timeout) {
    if (!isValid()) return;
#ifdef _WIN32
    DWORD ms = static_cast<DWORD>(timeout.count());
    setsockopt(handle_, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<const char*>(&ms), sizeof(ms));
#else
    struct timeval tv;
    tv.tv_sec = timeout.count() / 1000;
    tv.tv_usec = (timeout.count() % 1000) * 1000;
    setsockopt(handle_, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<const char*>(&tv), sizeof(tv));
#endif
}

void TcpSocket::setKeepAlive(bool enable) {
    applySocketOption(SOL_SOCKET, SO_KEEPALIVE, enable ? 1 : 0);
}

void TcpSocket::setNoDelay(bool enable) {
    applySocketOption(IPPROTO_TCP, TCP_NODELAY, enable ? 1 : 0);
}

void TcpSocket::setReceiveBufferSize(int bytes) {
    applySocketOption(SOL_SOCKET, SO_RCVBUF, bytes);
}

bool TcpSocket::sendAll(const uint8_t* data, size_t len) {
    if (!isValid()) return false;
    size_t totalSent = 0;
    while (totalSent < len) {
        auto sent = ::send(handle_, reinterpret_cast<const char*>(data + totalSent), static_cast<int>(len - totalSent), 0);
        if (sent < 0) {
#ifdef _WIN32
            int err = WSAGetLastError();
            if (err == WSAEWOULDBLOCK || err == WSAEINTR) continue;
#else
            if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) continue;
#endif
            return false;
        }
        totalSent += sent;
    }
    return true;
}

bool TcpSocket::sendAll(const std::vector<uint8_t>& data) {
    return sendAll(data.data(), data.size());
}

ssize_t TcpSocket::recv(uint8_t* buffer, size_t bufferSize) {
    if (!isValid()) return -1;
    auto r = ::recv(handle_, reinterpret_cast<char*>(buffer), static_cast<int>(bufferSize), 0);
    if (r < 0) {
#ifdef _WIN32
        int err = WSAGetLastError();
        if (err == WSAEWOULDBLOCK || err == WSAEINTR) return 0;
#else
        if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) return 0;
#endif
        return -1;
    }
    return r;
}

void TcpSocket::close() {
    if (isValid()) {
#ifdef _WIN32
        ::closesocket(handle_);
#else
        ::close(handle_);
#endif
        handle_ = kInvalidSocket;
    }
}

std::string TcpSocket::localAddress() const {
    if (!isValid()) return "";
    struct sockaddr_in addr;
    socklen_t len = sizeof(addr);
    if (getsockname(handle_, reinterpret_cast<struct sockaddr*>(&addr), &len) == 0) {
        char buf[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &addr.sin_addr, buf, sizeof(buf));
        return buf;
    }
    return "";
}

uint16_t TcpSocket::localPort() const {
    if (!isValid()) return 0;
    struct sockaddr_in addr;
    socklen_t len = sizeof(addr);
    if (getsockname(handle_, reinterpret_cast<struct sockaddr*>(&addr), &len) == 0) {
        return ntohs(addr.sin_port);
    }
    return 0;
}

std::string TcpSocket::remoteAddress() const {
    if (!isValid()) return "";
    struct sockaddr_in addr;
    socklen_t len = sizeof(addr);
    if (getpeername(handle_, reinterpret_cast<struct sockaddr*>(&addr), &len) == 0) {
        char buf[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &addr.sin_addr, buf, sizeof(buf));
        return buf;
    }
    return "";
}

uint16_t TcpSocket::remotePort() const {
    if (!isValid()) return 0;
    struct sockaddr_in addr;
    socklen_t len = sizeof(addr);
    if (getpeername(handle_, reinterpret_cast<struct sockaddr*>(&addr), &len) == 0) {
        return ntohs(addr.sin_port);
    }
    return 0;
}

SocketHandle TcpSocket::release() {
    SocketHandle h = handle_;
    handle_ = kInvalidSocket;
    return h;
}

} // namespace lantalk
