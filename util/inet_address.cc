#include "iomgr/tcp/inet_address.h"

#include <arpa/inet.h>
#include <glog/logging.h>
#include <netinet/in.h>
#include <stdlib.h>

#include "util/sockaddr_storage.h"

#define IPV4_DELIM '.'
#define IPV4_ADDR_SIZE 4
#define IPV6_DELIM ':'
#define IPV6_ADDR_SIZE 16

namespace iomgr {

static_assert(InetAddress::kIPAny == AF_UNSPEC);
static_assert(InetAddress::kIPv4 == AF_INET);
static_assert(InetAddress::kIPv6 == AF_INET6);

static std::string ToIPv4Address(const uint8_t bytes[IPV4_ADDR_SIZE]) {
  std::string out;
  for (int i = 0; i < 4; ++i) {
    out += std::to_string(bytes[i]);
    if (i != 3) {
      out.push_back(IPV4_DELIM);
    }
  }
  return out;
}

void ChooseIPv6ContractionRange(const uint8_t bytes[IPV6_ADDR_SIZE],
                                std::pair<int, int> &range) {
  // range(begin, len)
  std::pair<int, int> max_range = {0, -1}, cur_range = {0, -1};
  for (int i = 0; i < IPV6_ADDR_SIZE; i += 2) {
    DCHECK(i % 2 == 0);
    bool is_zero = (bytes[i] == 0 && bytes[i + 1] == 0);
    if (is_zero) {
      if (cur_range.second == -1) {  // invalid
        cur_range.first = i;
        cur_range.second = 0;
      }
      cur_range.second += 2;
    }

    if (!is_zero || i == 14) {
      if (cur_range.second > 2 && cur_range.second > max_range.second) {
        max_range = cur_range;
      }
      cur_range = {0, -1};
    }
  }
  range = max_range;
}

static std::string ToIPv6Address(const uint8_t bytes[IPV6_ADDR_SIZE]) {
  std::string out = "[";
  std::pair<int, int> range;
  ChooseIPv6ContractionRange(bytes, range);
  for (int i = 0; i <= IPV6_ADDR_SIZE - 2;) {
    DCHECK(i % 2 == 0);
    if (i == range.first && range.second > 0) {
      if (i == 0) {
        out.push_back(IPV6_DELIM);
      }
      out.push_back(IPV6_DELIM);
      i = range.first + range.second;
    } else {
      int x = bytes[i] << 8 | bytes[i + 1];
      i += 2;

      char str[5];
      snprintf(str, sizeof(str), "%x", x);
      for (int ch = 0; str[ch] != 0; ++ch) {
        out.push_back(str[ch]);
      }
      if (i < 16) {
        out.push_back(IPV6_DELIM);
      }
    }
  }
  out.push_back(']');
  return out;
}

InetAddress::InetAddress() : bytes_(), port_(0), family_(kIPAny) {}

InetAddress::InetAddress(const Slice &ip, const uint16_t port, Family family)
    : InetAddress() {
  SockaddrStorage storage;
  switch (family) {
    case kIPv4:
      if (::inet_pton(AF_INET, ip.data(), &storage.addr_in->sin_addr) == 1) {
        memcpy(bytes_.data(), &storage.addr_in->sin_addr, IPV4_ADDR_SIZE);
        port_ = port;
        family_ = kIPv4;
      }
      break;
    case kIPv6:
      if (::inet_pton(AF_INET6, ip.data(), &storage.addr_in6->sin6_addr) == 1) {
        memcpy(bytes_.data(), &storage.addr_in6->sin6_addr, IPV6_ADDR_SIZE);
        port_ = port;
        family_ = kIPv6;
      }
      break;
    default:;
  }
}

std::string InetAddress::ip() const {
  std::string ret;
  if (kIPv4 == family()) {
    ret = ToIPv4Address(bytes_.data());
  } else if (kIPv6 == family()) {
    ret = ToIPv6Address(bytes_.data());
  }
  return ret;
}

}  // namespace iomgr
