#ifndef LIBIOMGR_UTIL_SOCKET_OP_H_
#define LIBIOMGR_UTIL_SOCKET_OP_H_

#include <sys/socket.h>

#include "iomgr/status.h"
#include "iomgr/statusor.h"

namespace iomgr {

class SocketOp {
 public:
  static StatusOr<int> socket(int family, int type, int protocol);
  static Status bind(int fd, const struct sockaddr* addr, socklen_t addrlen);
  static Status connect(int fd, const struct sockaddr* addr, socklen_t addrlen);
  static Status listen(int fd, int backlog);
  static StatusOr<int> accept(int fd, sockaddr* addr, socklen_t* addrlen);
  static StatusOr<int> recv(int fd, void *buf, size_t count, int flags);
  static Status shutdown(int fd, int how);
  static Status get_local_name(int fd, sockaddr* addr, socklen_t* addrlen);
  static Status get_peer_name(int fd, sockaddr* addr, socklen_t* addrlen);
  static Status set_nodelay(int fd, bool enable);
  static Status set_reuse_addr(int fd, bool enable);
  static Status set_keep_alive(int fd, bool enable, int delay);
  static Status set_receive_buffer_size(int fd, int size);
  static Status set_send_buffer_size(int fd, int size);
};

}  // namespace iomgr

#endif  // LIBIOMGR_UTIL_SOCKET_OP_H_