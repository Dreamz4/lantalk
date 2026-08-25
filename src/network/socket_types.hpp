#pragma once

#ifdef _WIN32
  #ifndef WIN32_LEAN_AND_MEAN
  #define WIN32_LEAN_AND_MEAN
  #endif
  #include <winsock2.h>
  #include <ws2tcpip.h>
  using SocketHandle = SOCKET;
  inline constexpr SocketHandle kInvalidSocket = INVALID_SOCKET;
#else
  #include <sys/socket.h>
  #include <netinet/in.h>
  #include <arpa/inet.h>
  #include <netdb.h>
  #include <unistd.h>
  #include <fcntl.h>
  #include <errno.h>
  using SocketHandle = int;
  inline constexpr SocketHandle kInvalidSocket = -1;
#endif

#include <string>
#include <system_error>

namespace lantalk {

// Returns the last socket error as a human-readable string
std::string lastSocketError();

// Platform socket initialization (for Winsock2 on Windows)
bool initializeSockets();
void cleanupSockets();

} // namespace lantalk
