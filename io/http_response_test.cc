#include "iomgr/http/http_response.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace iomgr {

TEST(HTTPResponse, Basic) {
  HTTPResponse resp;

  resp.set_status_code(HTTPStatusCode::kOk);
  EXPECT_EQ(HTTPStatusCode::kOk, resp.status_code());
}

TEST(HTTPResponse, ToString) {
  HTTPResponse resp;

  resp.set_version(HTTPVersion::kHTTP11);
  resp.set_status_code(HTTPStatusCode::kNotFound);
  resp.AddHeader("Hello", "World");
  resp.AppendBody("something");

  EXPECT_EQ(
      "HTTP/1.1 404 Not Found\r\n"
      "Hello: World\r\n"
      "\r\n"
      "something",
      resp.ToString());
}

}  // namespace iomgr

int main(int argc, char** argv) {
  testing::InitGoogleTest(&argc, argv);
  testing::InitGoogleMock(&argc, argv);
  return RUN_ALL_TESTS();
}