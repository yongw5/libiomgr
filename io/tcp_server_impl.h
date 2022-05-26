#ifndef LIBIOMGR_IO_TCP_SERVER_IMPL_H_
#define LIBIOMGR_IO_TCP_SERVER_IMPL_H_

#include "iomgr/io_watcher.h"
#include "iomgr/tcp/tcp_server.h"
#include "util/scoped_fd.h"
#include "util/sockaddr_storage.h"

namespace iomgr {

class SockaddrStorage;

class TCPServerImpl : public TCPServer, IOWatcher {
 public:
  TCPServerImpl();
  ~TCPServerImpl();
  Status Open(int family);
  Status Bind(const InetAddress& local);
  Status Listen(int backlog);
  Status Accept(std::unique_ptr<TCPClient>* socket,
                TCPAcceptCb callback) override;
  Status Accept(std::unique_ptr<TCPClient>* socket, TCPAcceptCb callback,
                InetAddress* remote) override;
  Status GetLocalAddress(InetAddress* local) const override;
  Status AllowAddressReuse();

 private:
  Status DoAccept(std::unique_ptr<TCPClient>* socket, InetAddress* remote);
  void OnFileReadable(int fd) override;
  void OnFileWritable(int fd) override;

  ScopedFD socket_fd_;
  mutable std::unique_ptr<SockaddrStorage> local_address_;

  IOWatcher::Controller accept_socket_controller_;
  TCPAcceptCb accept_callback_;
  std::unique_ptr<TCPClient>* accept_socket_;
  InetAddress* remote_;
  SockaddrStorage accepted_address_;
  bool pending_accept_;
};

}  // namespace iomgr

#endif  // LIBIOMGR_IO_TCP_SERVER_IMPL_H_