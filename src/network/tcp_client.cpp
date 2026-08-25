#include "tcp_client.hpp"
#include <cstring>

#ifdef _WIN32
  #include <winsock2.h>
  #include <ws2tcpip.h>
#else
  #include <sys/types.h>
  #include <sys/socket.h>
  #include <netdb.h>
  #include <arpa/inet.h>
  #include <unistd.h>
  #include <fcntl.h>
#endif

namespace lantalk {

std::optional<std::string> TcpClient::resolveHostname(const std::string& hostname) {
    struct addrinfo hints, *res;
    std::memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    if (getaddrinfo(hostname.c_str(), nullptr, &hints, &res) != 0) {
        return std::nullopt;
    }

    char ipStr[INET_ADDRSTRLEN];
    struct sockaddr_in* ipv4 = reinterpret_cast<struct sockaddr_in*>(res->ai_addr);
    inet_ntop(AF_INET, &(ipv4->sin_addr), ipStr, INET_ADDRSTRLEN);
    
    std::string result(ipStr);
    freeaddrinfo(res);
    return result;
}

std::unique_ptr<TcpSocket> TcpClient::connect(
    const std::string& host,
    uint16_t port,
    std::chrono::seconds timeout
) {
    auto resolvedIp = resolveHostname(host);
    if (!resolvedIp) {
        throw ConnectError("Failed to resolve hostname: " + host);
    }

    SocketHandle h = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (h == kInvalidSocket) {
        throw ConnectError("Failed to create socket: " + lastSocketError());
    }

    auto socket = std::make_unique<TcpSocket>(h);
    socket->setBlocking(false);

    struct sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, resolvedIp->c_str(), &addr.sin_addr);

    int res = ::connect(h, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr));
    if (res < 0) {
#ifdef _WIN32
        int err = WSAGetLastError();
        if (err != WSAEWOULDBLOCK) {
            throw ConnectError("Connect error: " + lastSocketError());
        }
#else
        if (errno != EINPROGRESS) {
            throw ConnectError("Connect error: " + lastSocketError());
        }
#endif

        fd_set writeFds;
        FD_ZERO(&writeFds);
        FD_SET(h, &writeFds);

        struct timeval tv;
        tv.tv_sec = timeout.count();
        tv.tv_usec = 0;

        res = select(static_cast<int>(h) + 1, nullptr, &writeFds, nullptr, &tv);
        if (res <= 0) {
            throw ConnectError(res == 0 ? "Connection timed out" : "Select error");
        }

        int soError = 0;
        socklen_t len = sizeof(soError);
        getsockopt(h, SOL_SOCKET, SO_ERROR, reinterpret_cast<char*>(&soError), &len);
        if (soError != 0) {
            throw ConnectError("Connection refused");
        }
    }

    socket->setBlocking(true);
    return socket;
}

} // namespace lantalk
