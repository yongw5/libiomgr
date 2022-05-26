#ifndef LIBIOMGR_INCLUDE_TCP_TCP_SERVER_H_
#define LIBIOMGR_INCLUDE_TCP_TCP_SERVER_H_

#include <functional>
#include <memory>

#include "iomgr/export.h"
#include "iomgr/status.h"

namespace iomgr {

class TCPClient;
class InetAddress;

class IOMGR_EXPORT TCPServer {
 public:
  using TCPAcceptCb = std::function<void(Status)>;
  struct Options {
    Options() : reuse_address(false), backlog(5) {}
    Options(bool reuse_address, int backlog)
        : reuse_address(reuse_address), backlog(backlog) {}

    bool reuse_address;
    int backlog;
  };

  TCPServer();
  virtual ~TCPServer();

  TCPServer(const TCPServer&) = delete;
  TCPServer& operator=(const TCPServer&) = delete;

  static Status Listen(const InetAddress& local, const Options& options,
                       std::unique_ptr<TCPServer>* server);
  virtual Status Accept(std::unique_ptr<TCPClient>* socket,
                        TCPAcceptCb callback) = 0;
  virtual Status Accept(std::unique_ptr<TCPClient>* socket,
                        TCPAcceptCb callback, InetAddress* address) = 0;
  virtual Status GetLocalAddress(InetAddress* address) const = 0;
};

}  // namespace iomgr

#endif  // LIBIOMGR_INCLUDE_TCP_TCP_SERVER_H_