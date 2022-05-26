#ifndef LIBIOMGR_UTIL_SOCKET_ADDRESS_H_
#define LIBIOMGR_UTIL_SOCKET_ADDRESS_H_

#include <netinet/in.h>

#include "iomgr/slice.h"
#include "iomgr/tcp/inet_address.h"

namespace iomgr {

struct SockaddrStorage {
  SockaddrStorage();
  SockaddrStorage(const Slice& ip, uint16_t port, bool ipv6 = false);
  explicit SockaddrStorage(const InetAddress& address);
  SockaddrStorage(const SockaddrStorage& other);
  void operator=(const SockaddrStorage& other);

  bool IsIPv4() const { return addr_len == sizeof(*addr_in); }
  bool IsIPv6() const { return addr_len == sizeof(*addr_in6); }
  bool IsValid() const { return IsIPv4() || IsIPv6(); }
  int address_family() const;

  InetAddress ToInetAddress() const;

  struct sockaddr_storage addr_storage;
  socklen_t addr_len;
  union {
    struct sockaddr* const addr;
    struct sockaddr_in* const addr_in;
    struct sockaddr_in6* const addr_in6;
  };
};

}  // namespace iomgr

#endif  // LIBIOMGR_UTIL_SOCKET_ADDRESS_H_