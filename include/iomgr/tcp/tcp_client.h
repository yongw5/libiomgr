#ifndef LIBIOMGR_INCLUDE_TCP_TCP_CLIENT_H_
#define LIBIOMGR_INCLUDE_TCP_TCP_CLIENT_H_

#include <functional>
#include <memory>

#include "iomgr/export.h"
#include "iomgr/status.h"
#include "iomgr/statusor.h"

namespace iomgr {

class IOBuffer;
class InetAddress;

class IOMGR_EXPORT TCPClient {
 public:
  using TCPConnectCb = std::function<void(Status)>;
  using TCPReadCb = std::function<void(StatusOr<int>)>;
  using TCPWriteCb = std::function<void(StatusOr<int>)>;

  struct Options {
    Options()
        : no_delay(false),
          keep_alive(false, 0),
          receive_buffer_size(0 /* no-setting */),
          send_buffer_size(0 /* no-setting */) {}

    bool no_delay;
    std::pair<bool, int> keep_alive;
    int receive_buffer_size;
    int send_buffer_size;
  };

  TCPClient();
  virtual ~TCPClient();

  TCPClient(const TCPClient&) = delete;
  TCPClient& operator=(const TCPClient&) = delete;

  static Status Connect(const InetAddress& remote, const Options& options,
                        TCPConnectCb callback, const InetAddress* local,
                        std::unique_ptr<TCPClient>* client);
  virtual StatusOr<int> Read(IOBuffer* buf, int buf_len,
                             TCPReadCb callback) = 0;
  virtual StatusOr<int> ReadIfReady(IOBuffer* buf, int buf_len,
                                    TCPReadCb callback) = 0;
  virtual Status CancelReadIfReady() = 0;
  virtual StatusOr<int> Write(IOBuffer* buf, int buf_len,
                              TCPWriteCb callback) = 0;
  virtual Status Disconnect() = 0;
  virtual bool IsConnected() const = 0;
  virtual Status GetLocalAddress(InetAddress* local) const = 0;
  virtual Status GetRemoteAddress(InetAddress* remote) const = 0;
};

}  // namespace iomgr

#endif  // LIBIOMGR_INCLUDE_TCP_TCP_CLIENT_H_