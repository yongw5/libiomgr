#include "iomgr/http/http_server.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "iomgr/http/http_client.h"
#include "iomgr/http/http_request.h"
#include "iomgr/http/http_response.h"
#include "iomgr/tcp/inet_address.h"
#include "iomgr/tcp/tcp_server.h"
#include "threading/thread.h"
#include "util/notification.h"

namespace iomgr {

static const int kDefaultPort = 9997;

class TestDelegate : public HTTPServer::Delegate {
 public:
  void OnHTTPRequest(const HTTPRequest& request,
                     HTTPResponse& response) override {
    response = HTTPResponse::Ok();
  }
};

class HelperThread : public Thread {
 public:
  HelperThread(const InetAddress& addr, Notification* notification)
      : address_(addr), notification_(notification) {}

 private:
  void ThreadEntry() override {
    http_.reset(new HTTPServer(address_, &delegate_));
    notification_->WaitForNotification();
  }

  InetAddress address_;
  Notification* notification_;
  TestDelegate delegate_;
  std::unique_ptr<HTTPServer> http_;
};

class HTTPServerTest : public testing::Test {
 protected:
  void SetUp() override {
    InetAddress addr("0.0.0.0", kDefaultPort);
    thread_.reset(new HelperThread(addr, &notification_));
    thread_->StartThread();
  }

  void TearDown() override {
    notification_.Notify();
    thread_->StopThread();
  }

 private:
  Notification notification_;
  std::unique_ptr<HelperThread> thread_;
};

class HTTPCli {
 public:
  void WaitResponse() {
    notification_.WaitForNotification();
    DCHECK(response_.status_code() == kOk);
  }
  void OnResponse(Status status) { notification_.Notify(); }
  void SendRequest() {
    InetAddress remote("0.0.0.0", kDefaultPort);
    HTTPRequest request;
    request.set_method(HTTPMethod::kPut);
    request.set_uri("/path/to/home");
    request.set_version(HTTPVersion::kHTTP11);
    request.AddHeader("Hello", "World");
    request.AppendBody("something");

    HTTPClient::IssueRequest(
        remote, request,
        std::bind(&HTTPCli::OnResponse, this, std::placeholders::_1),
        &response_);
  }

 private:
  Notification notification_;
  HTTPResponse response_;
};

TEST_F(HTTPServerTest, Request) {
  HTTPCli client;
  client.SendRequest();
  client.WaitResponse();
}

}  // namespace iomgr

int main(int argc, char** argv) {
  testing::InitGoogleTest(&argc, argv);
  testing::InitGoogleMock(&argc, argv);
  return RUN_ALL_TESTS();
}