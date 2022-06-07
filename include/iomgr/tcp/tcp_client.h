#ifndef LIBIOMGR_INCLUDE_TCP_TCP_CLIENT_H_
#define LIBIOMGR_INCLUDE_TCP_TCP_CLIENT_H_

#include <functional>
#include <memory>

#include "iomgr/callback.h"
#include "iomgr/export.h"
#include "iomgr/status.h"
#include "iomgr/statusor.h"
#include "iomgr/time.h"

namespace iomgr {

class IOBuffer;
class InetAddress;

class IOMGR_EXPORT TCPClient {
 public:
  struct Options {
    Options()
        : no_delay(false),
          keep_alive(false, 0),
          connect_timeout(Time::Delta::Inifinite()),
          receive_buffer_size(0 /* no-setting */),
          send_buffer_size(0 /* no-setting */) {}

    bool no_delay;
    std::pair<bool, int> keep_alive;
    Time::Delta connect_timeout;
    int receive_buffer_size;
    int send_buffer_size;
  };

  TCPClient();
  virtual ~TCPClient();

  TCPClient(const TCPClient&) = delete;
  TCPClient& operator=(const TCPClient&) = delete;

  // Called to establish a connection.
  // Returns Status::OK() if the connection is established immediately.
  // Otherwise, Status::TryAgain() is returned and the connect_callback will be
  // run when the connection is established or when an error occurs.
  static Status Connect(const InetAddress& remote, const Options& options,
                        StatusCallback connect_callback,
                        const InetAddress* local,
                        std::unique_ptr<TCPClient>* client);

  // Called to read data from connection, up to |buf_len| bytes.
  // Returns number of bytes read from connection, or zero if received EOF.
  // Otherwise, return Status::TryAgain() and read_callback will be run when
  // after data is read from socket.
  virtual StatusOr<int> Read(IOBuffer* buf, int buf_len,
                             StatusOrIntCallback read_callback) = 0;

  // Called to read data from connection, up to |read_buf| bytes.
  // Return number of bytes read from connection, or zero if received EOF.
  // Otherwise, return Status::TryAgain() and read_callback will be run when
  // data arraived.
  virtual StatusOr<int> ReadIfReady(IOBuffer* buf, int buf_len,
                                    StatusCallback read_callback) = 0;

  // Cancels a pending ReadIfReady()
  virtual Status CancelReadIfReady() = 0;

  // Called to write data to connection, up to |buf_len| bytes.
  // Return number of bytes write to connection, or Status::TryAgain() if data
  // cannot be send immediately. And write_callback will be run when all data
  // have been send or an error occurs.
  virtual StatusOr<int> Write(IOBuffer* buf, int buf_len,
                              StatusOrIntCallback callback) = 0;
  virtual Status Disconnect() = 0;
  virtual bool IsConnected() const = 0;
  virtual Status GetLocalAddress(InetAddress* local) const = 0;
  virtual Status GetRemoteAddress(InetAddress* remote) const = 0;
};

}  // namespace iomgr

#endif  // LIBIOMGR_INCLUDE_TCP_TCP_CLIENT_H_