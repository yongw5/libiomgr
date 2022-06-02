#include "util/http_parser.h"

#include <stdarg.h>

#include "glog/logging.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "iomgr/http/http_request.h"
#include "iomgr/http/http_response.h"

namespace iomgr {

class HTTPParserTest : public testing::Test {
 protected:
  void TestRequestSucceeds(const Slice& request_test, HTTPMethod expect_method,
                           HTTPVersion expect_version, const Slice& expect_uri,
                           const Slice& expect_body, ...) {
    HTTPRequest req;
    HTTPParser<HTTPRequest> parser(&req);

    std::vector<Slice> parts = SplitSlice(request_test);
    for (auto& slice : parts) {
      EXPECT_TRUE(parser.Parse(slice, nullptr));
    }

    EXPECT_TRUE(parser.RecievedAllHeaders());

    EXPECT_EQ(expect_method, req.method());
    EXPECT_EQ(expect_version, req.version());
    EXPECT_EQ(expect_uri, req.uri());
    EXPECT_EQ(expect_body, req.body());

    va_list args;
    va_start(args, expect_body);
    for (int i = 0; true; ++i) {
      const char* expect_key = va_arg(args, char*);
      if (!expect_key) {
        break;
      }
      const char* expect_value = va_arg(args, char*);
      EXPECT_TRUE(expect_value);

      EXPECT_EQ(expect_key, req.headers()[i].key);
      EXPECT_EQ(expect_value, req.headers()[i].value);
    }
  }

  void TestResponseSucceeds(const Slice& repsonse_text,
                            HTTPVersion expect_version,
                            HTTPStatusCode expect_status,
                            const Slice& expect_body, ...) {
    HTTPResponse res;
    HTTPParser<HTTPResponse> parser(&res);

    std::vector<Slice> parts = SplitSlice(repsonse_text);
    for (auto& slice : parts) {
      EXPECT_TRUE(parser.Parse(slice, nullptr));
    }

    EXPECT_TRUE(parser.RecievedAllHeaders());

    EXPECT_EQ(expect_status, res.status_code());
    EXPECT_EQ(expect_version, res.version());
    EXPECT_EQ(expect_body, res.body());

    va_list args;
    va_start(args, expect_body);
    for (int i = 0; true; ++i) {
      const char* expect_key = va_arg(args, char*);
      if (!expect_key) {
        break;
      }
      const char* expect_value = va_arg(args, char*);
      EXPECT_TRUE(expect_value);

      EXPECT_EQ(expect_key, res.headers()[i].key);
      EXPECT_EQ(expect_value, res.headers()[i].value);
    }
  }

  void TestRequestFails(const Slice& request_text) {
    HTTPRequest req;
    HTTPParser<HTTPRequest> parser(&req);

    std::vector<Slice> parts = SplitSlice(request_text);
    bool no_error = true;
    for (auto& slice : parts) {
      if (!(no_error = parser.Parse(slice, nullptr))) {
        break;
      }
    }
    if (no_error) {
      no_error = parser.RecievedAllHeaders();
    }

    EXPECT_FALSE(no_error);
  }

  void TestResponseFails(const Slice& response_text) {
    HTTPResponse res;
    HTTPParser<HTTPResponse> parser(&res);

    std::vector<Slice> parts = SplitSlice(response_text);
    bool no_error = true;
    for (auto& slice : parts) {
      if (!(no_error = parser.Parse(slice, nullptr))) {
        break;
      }
    }
    if (no_error) {
      no_error = parser.RecievedAllHeaders();
    }

    EXPECT_FALSE(no_error);
  }

 private:
  static std::vector<Slice> SplitSlice(Slice slice) {
    std::vector<Slice> ret;
    while (!slice.empty()) {
      size_t len = random() % slice.size() + 1;
      ret.push_back(Slice(slice.data(), len));
      slice.remove_prefix(len);
    }
    return ret;
  }
};

TEST_F(HTTPParserTest, Ctor) {
  {
    HTTPRequest req;
    HTTPParser<HTTPRequest> req_parser(&req);
  }
  {
    HTTPResponse res;
    HTTPParser<HTTPResponse> req_parser(&res);
  }
}

TEST_F(HTTPParserTest, TestRequestSucceeds) {
  TestRequestSucceeds(
      "GET / HTTP/1.0\r\n"
      "\r\n",
      HTTPMethod::kGet, HTTPVersion::kHTTP10, "/", "", NULL);
  TestRequestSucceeds(
      "POST / HTTP/1.1\r\n"
      "\r\n"
      "abc",
      HTTPMethod::kPost, HTTPVersion::kHTTP11, "/", "abc", NULL);
  TestRequestSucceeds(
      "DELETE /file.html HTTP/2.0\r\n"
      "\r\n"
      "xy",
      HTTPMethod::kDelete, HTTPVersion::kHTTP20, "/file.html", "xy", NULL);
  TestRequestSucceeds(
      "PUT /new.html HTTP/1.1\r\n"
      "\r\n"
      "xyzzz",
      HTTPMethod::kPut, HTTPVersion::kHTTP11, "/new.html", "xyzzz", NULL);
  TestRequestSucceeds(
      "GET / HTTP/1.0\r\n"
      "xyz: abc\r\n"
      "\r\n"
      "xyz",
      HTTPMethod::kGet, HTTPVersion::kHTTP10, "/", "xyz", "xyz", "abc", NULL);
  TestRequestSucceeds(
      "GET / HTTP/1.0\n"
      "\n"
      "xyz",
      HTTPMethod::kGet, HTTPVersion::kHTTP10, "/", "xyz", NULL);
}

TEST_F(HTTPParserTest, TestResponseSucceeds) {
  TestResponseSucceeds(
      "HTTP/1.0 200 OK\r\n"
      "xyz: abc\r\n"
      "\r\n"
      "hello world!",
      HTTPVersion::kHTTP10, HTTPStatusCode::kOk, "hello world!", "xyz", "abc",
      NULL);
  TestResponseSucceeds(
      "HTTP/1.0 404 Not Found\r\n"
      "\r\n",
      HTTPVersion::kHTTP10, HTTPStatusCode::kNotFound, "", NULL);
  TestResponseSucceeds(
      "HTTP/1.1 200 OK\r\n"
      "xyz: abc\r\n"
      "\r\n"
      "hello world!",
      HTTPVersion::kHTTP11, HTTPStatusCode::kOk, "hello world!", "xyz", "abc",
      NULL);
  TestResponseSucceeds(
      "HTTP/1.1 200 OK\n"
      "\n"
      "abc",
      HTTPVersion::kHTTP11, HTTPStatusCode::kOk, "abc", NULL);
}

TEST_F(HTTPParserTest, TestRequestFails) {
  TestRequestFails("GET\r\n");
  TestRequestFails("GET /\r\n");
  TestRequestFails("GET / HTTP/0.0\r\n");
  TestRequestFails("GET / ____/1.0\r\n");
  TestRequestFails("GET / HTTP/1.2\r\n");
  TestRequestFails("GET / HTTP/1.0\n");
}

TEST_F(HTTPParserTest, TestResponseFails) {
  TestResponseFails("HTTP/1.0\r\n");
  TestResponseFails("HTTP/1.2\r\n");
  TestResponseFails("HTTP/1.0 000 XYX\r\n");
  TestResponseFails("HTTP/1.0 200 OK\n");
  TestResponseFails("HTTP/1.0 200 OK\r\n");
  TestResponseFails("HTTP/1.0 200 OK\r\nFoo x\r\n");
  TestResponseFails(
      "HTTP/1.0 200 OK\r\n"
      "xyz: abc\r\n"
      "  def\r\n"
      "\r\n"
      "hello world!");
}

}  // namespace iomgr

int main(int argc, char** argv) {
  testing::InitGoogleTest(&argc, argv);
  testing::InitGoogleMock(&argc, argv);
  return RUN_ALL_TESTS();
}