#include "util/sockaddr_storage.h"

#include <arpa/inet.h>
#include <glog/logging.h>
#include <gtest/gtest.h>

#define NS_INT16SZ 2
#define NS_INADDRSZ 4
#define NS_IN6ADDRSZ 16

#define IPV4_DELIM '.'
#define IPV4_ADDR_SIZE 4
#define IPV6_DELIM ':'
#define IPV6_ADDR_SIZE 16
#define TWO_BYTE_SIZE 2

namespace iomgr {

struct TestData {
  Slice ip;
  Slice ip_norm;
  uint16_t port;
  bool ipv6;
} tests[] = {
    {"127.0.0.1", "127.0.0.1", 80, false},
    {"192.168.1.1", "192.168.1.1", 90, false},
    {"::1", "[::1]", 100, true},
    {"2001:db8:0::42", "[2001:db8::42]", 110, true},
};

TEST(InetAddressTest, ConstructEmpty) {
  InetAddress addr;
  EXPECT_TRUE(addr.ip().empty());
  EXPECT_EQ(addr.port(), 0);
  EXPECT_EQ(addr.family(), InetAddress::kIPAny);
}

TEST(InetAddressTest, ConstructIPv4) {
  for (auto& test : tests) {
    if (!test.ipv6) {
      InetAddress addr(test.ip, test.port, InetAddress::kIPv4);
      EXPECT_EQ(addr.ip(), test.ip);
      EXPECT_EQ(addr.port(), test.port);
      EXPECT_EQ(addr.family(), InetAddress::kIPv4);
    }
  }
}

TEST(InetAddressTest, ConstructIPv6) {
  for (auto& test : tests) {
    if (test.ipv6) {
      InetAddress addr(test.ip, test.port, InetAddress::kIPv6);
      EXPECT_EQ(addr.ip(), test.ip_norm);
      EXPECT_EQ(addr.port(), test.port);
      EXPECT_EQ(addr.family(), InetAddress::kIPv6);
    }
  }
}

TEST(InetAddressTest, ConstructInvalid) {
  {
    Slice ip = "192.168..1";
    uint16_t port = 80;
    InetAddress addr(ip, port, InetAddress::kIPv4);
    EXPECT_TRUE(addr.ip().empty());
    EXPECT_EQ(addr.port(), 0);
    EXPECT_EQ(addr.family(), InetAddress::kIPAny);
  }

  {
    Slice ip = "2001:db8:0::42-";
    uint16_t port = 80;
    InetAddress addr(ip, port, InetAddress::kIPv6);
    EXPECT_TRUE(addr.ip().empty());
    EXPECT_EQ(addr.port(), 0);
    EXPECT_EQ(addr.family(), InetAddress::kIPAny);
  }
}

TEST(SockaddrStorageTest, ConstructEmpty) {
  SockaddrStorage storage;
  CHECK_EQ(AF_UNSPEC, storage.addr->sa_family);
  EXPECT_FALSE(storage.IsValid());

  CHECK_EQ(AF_UNSPEC, storage.addr_in->sin_family);
  CHECK_EQ(0, storage.addr_in->sin_port);
  CHECK_EQ(INADDR_ANY, storage.addr_in->sin_addr.s_addr);

  CHECK_EQ(AF_UNSPEC, storage.addr_in6->sin6_family);
  CHECK_EQ(0, storage.addr_in6->sin6_port);
  CHECK_EQ(0, memcmp(storage.addr_in6->sin6_addr.s6_addr, &in6addr_any,
                     sizeof(in6addr_any)));
}

TEST(SockaddrStorageTest, ConstructIPv4) {
  for (auto& test : tests) {
    if (!test.ipv6) {
      SockaddrStorage storage(test.ip, test.port, test.ipv6);

      EXPECT_TRUE(storage.IsIPv4());
      EXPECT_FALSE(storage.IsIPv6());
      EXPECT_TRUE(storage.IsValid());

      CHECK_EQ(AF_INET, storage.addr_in->sin_family);
      CHECK_EQ(::htons(test.port), storage.addr_in->sin_port);
      CHECK_EQ(::inet_addr(test.ip.data()), storage.addr_in->sin_addr.s_addr);
    }
  }
}

TEST(SockaddrStorageTest, ConstructIPv6) {
  for (auto& test : tests) {
    if (test.ipv6) {
      SockaddrStorage storage(test.ip, test.port, test.ipv6);

      EXPECT_FALSE(storage.IsIPv4());
      EXPECT_TRUE(storage.IsIPv6());
      EXPECT_TRUE(storage.IsValid());

      CHECK_EQ(AF_INET6, storage.addr_in->sin_family);
      CHECK_EQ(::htons(test.port), storage.addr_in->sin_port);
      struct in6_addr in6addr = IN6ADDR_ANY_INIT;
      CHECK_EQ(1, ::inet_pton(AF_INET6, test.ip.data(), &in6addr));
      CHECK_EQ(0, memcmp(storage.addr_in6->sin6_addr.s6_addr, &in6addr,
                         sizeof(in6addr)));
    }
  }
}

TEST(SockaddrStorageTest, ConstructFromInetAddress) {
  for (auto& test : tests) {
    if (test.ipv6) {
      continue;
    }

    InetAddress address(test.ip, test.port, InetAddress::kIPv4);
    SockaddrStorage storage(test.ip, test.port, test.ipv6);
    SockaddrStorage storage2(address);

    EXPECT_TRUE(storage2.IsIPv4());
    EXPECT_FALSE(storage2.IsIPv6());
    EXPECT_TRUE(storage2.IsValid());
    EXPECT_EQ(storage.addr_in->sin_family, storage2.addr_in->sin_family);
    EXPECT_EQ(storage.addr_in->sin_port, storage2.addr_in->sin_port);
    EXPECT_EQ(storage.addr_in->sin_addr.s_addr,
              storage2.addr_in->sin_addr.s_addr);
  }
}

TEST(SockaddrStorageTest, ConstructFromInetAddress2) {
  for (auto& test : tests) {
    if (!test.ipv6) {
      continue;
    }

    InetAddress address(test.ip, test.port, InetAddress::kIPv6);
    SockaddrStorage storage(test.ip, test.port, test.ipv6);
    SockaddrStorage storage2(address);

    EXPECT_FALSE(storage2.IsIPv4());
    EXPECT_TRUE(storage2.IsIPv6());
    EXPECT_TRUE(storage2.IsValid());
    EXPECT_EQ(storage.addr_in6->sin6_family, storage2.addr_in6->sin6_family);
    EXPECT_EQ(storage.addr_in6->sin6_port, storage2.addr_in6->sin6_port);
    EXPECT_TRUE(IN6_ARE_ADDR_EQUAL(storage.addr_in6, storage2.addr_in6));
  }
}

TEST(SockaddrStorageTest, IsValid) {
  {
    Slice ip = "192.168..1";
    uint16_t port = 80;
    SockaddrStorage storage(ip, port, false);
    EXPECT_FALSE(storage.IsIPv4());
    EXPECT_FALSE(storage.IsIPv6());
    EXPECT_FALSE(storage.IsValid());
  }

  {
    Slice ip = "2001:db8:0::42-";
    uint16_t port = 80;
    SockaddrStorage storage(ip, port, true);
    EXPECT_FALSE(storage.IsIPv4());
    EXPECT_FALSE(storage.IsIPv6());
    EXPECT_FALSE(storage.IsValid());
  }
}

TEST(SockaddrStorageTest, Assignment) {
  for (const auto& test : tests) {
    SockaddrStorage src(test.ip, test.port, test.ipv6);
    SockaddrStorage dst = src;

    CHECK_EQ(src.addr_len, dst.addr_len);
    EXPECT_TRUE(!memcmp(src.addr, dst.addr, dst.addr_len));
  }
}

TEST(SockaddrStorageTest, Copy) {
  for (const auto& test : tests) {
    SockaddrStorage src(test.ip, test.port, test.ipv6);
    SockaddrStorage dst(src);

    CHECK_EQ(src.addr_len, dst.addr_len);
    EXPECT_TRUE(!memcmp(src.addr, dst.addr, dst.addr_len));
  }
}

TEST(SockaddrStorageTest, ToInetAddress) {
  for (auto& test : tests) {
    InetAddress::Family family = InetAddress::kIPv4;
    if (test.ipv6) {
      family = InetAddress::kIPv6;
    }
    InetAddress addr(test.ip, test.port, family);
    SockaddrStorage storage(test.ip, test.port, test.ipv6);

    InetAddress addr2 = storage.ToInetAddress();

    EXPECT_EQ(addr, addr2);
  }
}

}  // namespace iomgr

int main(int argc, char** argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
