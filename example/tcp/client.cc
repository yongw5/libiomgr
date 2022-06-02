#include <condition_variable>
#include <functional>
#include <mutex>

#include "glog/logging.h"
#include "iomgr/io_buffer.h"
#include "iomgr/tcp/inet_address.h"
#include "iomgr/tcp/tcp_client.h"

namespace iomgr {

class HelloWorldClient {
 public:
  HelloWorldClient(const std::string& ip, uint16_t port)
      : remote_(ip, port), connect_done_(false), read_done_(false) {
    Status status = TCPClient::Connect(
        remote_, TCPClient::Options(),
        std::bind(&HelloWorldClient::OnConnected, this, std::placeholders::_1),
        nullptr, &client_);
    if (status.ok()) {
      connect_done_ = true;
    }
  }
  ~HelloWorldClient() { client_->Disconnect(); }

  void Start() {
    {
      std::unique_lock<std::mutex> lock(mutex_);
      while (!connect_done_) {
        cv_.wait(lock);
      }
    }
    RefPtr<IOBufferWithSize> buf = MakeRefCounted<IOBufferWithSize>(128);
    StatusOr<int> status_or =
        client_->Read(buf.get(), buf->size(),
                      std::bind(&HelloWorldClient::OnReadCompleted, this,
                                buf.get(), std::placeholders::_1));
    if (!status_or.ok()) {
      std::unique_lock<std::mutex> lock(mutex_);
      cv_.wait(lock);
    } else {
      std::string msg(buf->data(), status_or.value());
      LOG(ERROR) << "[FROM SRV] " << msg;
    }
  }

 private:
  void OnConnected(Status status) {
    DCHECK(status.ok());
    if (!connect_done_) {
      connect_done_ = true;
      cv_.notify_one();
    }
  }

  void OnReadCompleted(IOBuffer* buf, StatusOr<int> status_or) {
    DCHECK(status_or.ok());
    std::string msg(buf->data(), status_or.value());
    LOG(ERROR) << "[FROM SRV] " << msg;
  }

  InetAddress remote_;
  std::unique_ptr<TCPClient> client_;
  std::mutex mutex_;
  std::condition_variable cv_;
  bool connect_done_;
  bool read_done_;
};

}  // namespace iomgr

int main() {
  iomgr::HelloWorldClient client("0.0.0.0", 9999);
  client.Start();
  return 0;
}