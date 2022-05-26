#ifndef LIBIOMGR_IO_TCP_CLIENT_IMPL_H_
#define LIBIOMGR_IO_TCP_CLIENT_IMPL_H_

#include "iomgr/io_watcher.h"
#include "iomgr/ref_counted.h"
#include "iomgr/tcp/tcp_client.h"
#include "util/scoped_fd.h"

namespace iomgr {

class SockaddrStorage;

class TCPClientImpl : public TCPClient, IOWatcher {
 public:
  TCPClientImpl();
  ~TCPClientImpl() override;

  TCPClientImpl(const TCPClientImpl&) = delete;
  TCPClientImpl& operator=(const TCPClientImpl&) = delete;

  Status Open(int family);
  Status Bind(const InetAddress& local);
  Status AdoptConnectedSocket(int socket, const InetAddress& remote);
  Status Connect(const InetAddress& remote,
                 std::function<void(Status)> connect_callback);
  StatusOr<int> Read(IOBuffer* buf, int buf_len,
                     TCPReadCb callback) override;
  StatusOr<int> ReadIfReady(IOBuffer* buf, int buf_len,
                            TCPReadCb callback) override;
  Status CancelReadIfReady() override;
  StatusOr<int> Write(IOBuffer* buf, int buf_len,
                      TCPWriteCb callback) override;
  StatusOr<int> WriteWhenReady(IOBuffer* buf, int buf_len,
                               TCPWriteCb callback);
  Status Disconnect() override;
  bool IsConnected() const override;
  Status GetLocalAddress(InetAddress* local) const override;
  Status GetRemoteAddress(InetAddress* remote) const override;
  Status SetKeepAlive(bool enable, int delay);
  Status SetNoDelay(bool on_delay);
  Status SetReceiveBufferSize(int size);
  Status SetSendBufferSize(int size);
  int ReleaseSocketFdForTesting() { return socket_fd_.release(); }

 private:
  enum ConnectState {
    kNone,
    kConnecting,
    kConnected,
  };

  Status DoConnect();
  StatusOr<int> DoRead(IOBuffer* buf, int buf_len);
  StatusOr<int> DoWrite(IOBuffer* buf, int buf_len);
  void RetryRead(StatusOr<int> ret);
  void OnConnectDone();
  void OnReadDone();
  void OnWriteDone();
  void OnFileReadable(int fd) override;
  void OnFileWritable(int fd) override;

  ScopedFD socket_fd_;

  IOWatcher::Controller connect_socket_controller_;
  TCPConnectCb connect_callback_;
  ConnectState connect_state_;

  IOWatcher::Controller read_socket_controller_;
  // Non-null when a Read() is in progress.
  RefPtr<IOBuffer> read_buf_;
  int read_buf_len_;
  TCPReadCb read_callback_;
  // Non-null when a ReadIfReady() is in progress
  TCPReadCb read_if_ready_callback_;

  IOWatcher::Controller write_socket_controller_;
  RefPtr<IOBuffer> write_buf_;
  int write_buf_len_;
  TCPWriteCb write_callback_;

  mutable std::unique_ptr<SockaddrStorage> local_address_;
  std::unique_ptr<SockaddrStorage> remote_address_;
};

}  // namespace iomgr

#endif