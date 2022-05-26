#include <glog/logging.h>

#include <condition_variable>
#include <functional>
#include <mutex>

#include "iomgr/io_buffer.h"
#include "iomgr/tcp/inet_address.h"
#include "iomgr/tcp/tcp_client.h"
#include "iomgr/tcp/tcp_server.h"

namespace iomgr {

class HelloWorldServer {
 public:
  HelloWorldServer(std::string ip, uint16_t port) : local_(ip, port) {}

  ~HelloWorldServer() { Stop(); }

  void Start() {
    Status status =
        TCPServer::Listen(local_, iomgr::TCPServer::Options(), &server_);
    DCHECK(status.ok());
    LOG(INFO) << "Listen on " << local_.ip() << ":" << local_.port();

    DoAcceptLoop();

    std::unique_lock<std::mutex> lock(mutex_);
    cv_.wait(lock);
  }
  void Stop() { cv_.notify_all(); }

 private:
  void DoAcceptLoop() {
    Status status;
    do {
      status = server_->Accept(
          &client_, std::bind(&HelloWorldServer::OnAcceptCompleted, this,
                              std::placeholders::_1));
      if (status.IsTryAgain()) {
        return;
      }
      status = HandleAcceptResult(status);
    } while (status.ok());
  }

  void OnAcceptCompleted(Status status) {
    if (HandleAcceptResult(status).ok()) {
      DoAcceptLoop();
    } else {
      Stop();
    }
  }

  Status HandleAcceptResult(Status status) {
    if (!status.ok()) {
      return status;
    }
    std::string msg = "HelloWorld";
    RefPtr<StringIOBuffer> iobuf = MakeRefCounted<StringIOBuffer>(msg);

    StatusOr<int> status_or =
        client_->Write(iobuf.get(), iobuf->size(),
                       std::bind(&HelloWorldServer::OnWriteCompleted, this,
                                 client_.release(), std::placeholders::_1));
    return status_or.status();
  }

  void OnWriteCompleted(TCPClient* client, StatusOr<int> status_or) {
    std::unique_ptr<TCPClient> cli(client);
    DCHECK(status_or.ok());
    cli->Disconnect();
  }

  void Wait() {
    std::unique_lock<std::mutex> lock(mutex_);
    cv_.wait(lock);
    lock.release();
  }

  InetAddress local_;
  std::unique_ptr<TCPServer> server_;
  std::unique_ptr<TCPClient> client_;
  std::mutex mutex_;
  std::condition_variable cv_;
};

}  // namespace iomgr

int main() {
  iomgr::HelloWorldServer server("0.0.0.0", 9999);
  server.Start();
  return 0;
}