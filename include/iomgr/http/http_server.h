#ifndef LIBIOMGR_HTTP_HTTP_SERVER_H_
#define LIBIOMGR_HTTP_HTTP_SERVER_H_

#include <map>
#include <memory>

#include "iomgr/export.h"
#include "iomgr/status.h"

namespace iomgr {

class TCPServer;
class TCPClient;
class HTTPRequest;
class HTTPResponse;
class InetAddress;
class Notification;

class IOMGR_EXPORT HTTPServer {
 public:
  class Delegate {
   public:
    virtual ~Delegate() = default;
    virtual void OnHTTPRequest(const HTTPRequest& request,
                               HTTPResponse& response) = 0;
  };

  HTTPServer(const InetAddress& addr, HTTPServer::Delegate* delegate);
  ~HTTPServer();

  HTTPServer(const HTTPServer&) = delete;
  HTTPServer& operator=(const HTTPServer&) = delete;

 private:
  class HTTPServerTest;

  void DoAcceptLoop();
  void OnAcceptCompleted(Status status);
  bool HandleAcceptResult(Status status);

  std::unique_ptr<TCPServer> server_;
  std::unique_ptr<TCPClient> accepted_socket_;
  HTTPServer::Delegate* const delegate_;
};

}  // namespace iomgr

#endif