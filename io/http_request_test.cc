#include "iomgr/http/http_request.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace iomgr {

TEST(HTTPRequest, Basic) {
  HTTPRequest req;

  req.set_method(HTTPMethod::kGet);
  req.set_uri("/path/to/something");

  EXPECT_EQ(req.method(), HTTPMethod::kGet);
  EXPECT_EQ(req.uri(), "/path/to/something");
}

TEST(HTTPRequest, ToString) {
  HTTPRequest req;

  req.set_method(HTTPMethod::kPut);
  req.set_uri("/path/to/home");
  req.set_version(HTTPVersion::kHTTP11);
  req.AddHeader("Hello", "World");
  req.AppendBody("something");

  EXPECT_EQ(
      "PUT /path/to/home HTTP/1.1\r\n"
      "Hello: World\r\n"
      "content-length: 9\r\n"
      "\r\n"
      "something",
      req.ToString());
}

}  // namespace iomgr

int main(int argc, char** argv) {
  testing::InitGoogleTest(&argc, argv);
  testing::InitGoogleMock(&argc, argv);
  return RUN_ALL_TESTS();
}