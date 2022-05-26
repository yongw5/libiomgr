#ifndef LIBIOMGR_INCLUDE_TCP_INET_ADDRESS_H_
#define LIBIOMGR_INCLUDE_TCP_INET_ADDRESS_H_

#include <array>
#include <string>

#include "iomgr/export.h"
#include "iomgr/slice.h"

namespace iomgr {

class IOMGR_EXPORT InetAddress {
 public:
  enum Family {
    kIPAny = 0,
    kIPv4 = 2,
    kIPv6 = 10,
  };

  InetAddress();
  InetAddress(const Slice& ip, const uint16_t port, Family family = kIPv4);

  std::string ip() const;
  uint16_t port() const { return port_; }
  Family family() const { return family_; }

 private:
  friend class SockaddrStorage;

  std::array<uint8_t, 16> bytes_;
  uint16_t port_;
  Family family_;
};

inline bool operator==(const InetAddress& x, const InetAddress& y) {
  return (x.ip() == y.ip()) && (x.port() == y.port()) &&
         (x.family() == y.family());
}

inline bool operator!=(const InetAddress& x, const InetAddress& y) {
  return !(x == y);
}

}  // namespace iomgr

#endif  // LIBIOMGR_INCLUDE_TCP_INET_ADDRESS_H_