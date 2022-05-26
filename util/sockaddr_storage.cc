#include "util/sockaddr_storage.h"

#include <arpa/inet.h>
#include <string.h>

#define IPV4_ADDR_SIZE 4
#define IPV6_ADDR_SIZE 16
#define IPV6_DELIM ':'

namespace iomgr {

SockaddrStorage::SockaddrStorage()
    : addr_len(sizeof(addr_storage)),
      addr(reinterpret_cast<struct sockaddr*>(&addr_storage)) {
  memset(addr, 0, addr_len);
}

SockaddrStorage::SockaddrStorage(const Slice& ip, uint16_t port, bool ipv6)
    : SockaddrStorage() {
  if (ipv6 || strchr(ip.data(), IPV6_DELIM)) {
    addr_in6->sin6_family = AF_INET6;
    addr_in6->sin6_port = ::htons(port);
    if (::inet_pton(AF_INET6, ip.data(), &addr_in6->sin6_addr) == 1) {
      addr_len = sizeof(*addr_in6);
    }
  } else {
    addr_in->sin_family = AF_INET;
    addr_in->sin_port = ::htons(port);
    if (::inet_pton(AF_INET, ip.data(), &addr_in->sin_addr) == 1) {
      addr_len = sizeof(*addr_in);
    }
  }
}

SockaddrStorage::SockaddrStorage(const InetAddress& address)
    : SockaddrStorage() {
  switch (address.family()) {
    case InetAddress::kIPv4:
      memcpy(&addr_in->sin_addr, address.bytes_.data(), IPV4_ADDR_SIZE);
      addr_in->sin_family = AF_INET;
      addr_in->sin_port = ::htons(address.port());
      addr_len = sizeof(*addr_in);
      break;
    case InetAddress::kIPv6:
      memcpy(&addr_in6->sin6_addr, address.bytes_.data(), IPV6_ADDR_SIZE);
      addr_in6->sin6_family = AF_INET6;
      addr_in6->sin6_port = ::htons(address.port());
      addr_len = sizeof(*addr_in6);
      break;
    default:;
  }
}

SockaddrStorage::SockaddrStorage(const SockaddrStorage& other)
    : addr_len(other.addr_len),
      addr(reinterpret_cast<struct sockaddr*>(&addr_storage)) {
  memcpy(addr, other.addr, addr_len);
}

void SockaddrStorage::operator=(const SockaddrStorage& other) {
  addr_len = other.addr_len;
  memcpy(addr, other.addr, addr_len);
}

int SockaddrStorage::address_family() const {
  int ret = AF_UNSPEC;
  if (IsValid()) {
    ret = addr->sa_family;
  }
  return ret;
}

InetAddress SockaddrStorage::ToInetAddress() const {
  InetAddress address;
  if (!IsValid()) {
    return address;
  }

  if (IsIPv4()) {
    memcpy(address.bytes_.data(), &addr_in->sin_addr, IPV4_ADDR_SIZE);
    address.port_ = ::ntohs(addr_in->sin_port);
    address.family_ = InetAddress::kIPv4;

  } else if (IsIPv6()) {
    memcpy(address.bytes_.data(), &addr_in6->sin6_addr, IPV6_ADDR_SIZE);
    address.port_ = ::ntohs(addr_in6->sin6_port);
    address.family_ = InetAddress::kIPv6;
  }

  return address;
}

}  // namespace iomgr
