#include "util/socket_op.h"

#include <netinet/in.h>
#include <netinet/tcp.h>

#include "util/os_error.h"

namespace iomgr {

StatusOr<int> SocketOp::socket(int family, int type, int protocol) {
  int socket_fd = ::socket(family, type, protocol);
  if (socket_fd == -1) {
    return StatusOr<int>(MapSystemError(errno));
  }
  return StatusOr<int>(socket_fd);
}

Status SocketOp::bind(int fd, const struct sockaddr* addr, socklen_t addrlen) {
  if (::bind(fd, addr, addrlen) == -1) {
    return MapSystemError(errno);
  }
  return Status();
}

Status SocketOp::connect(int fd, const struct sockaddr* addr,
                         socklen_t addrlen) {
  if (TEMP_FAILURE_RETRY(::connect(fd, addr, addrlen)) == -1) {
    return MapSocketConnectError(errno);
  }
  return Status();
}

Status SocketOp::listen(int fd, int backlog) {
  if (::listen(fd, backlog) == -1) {
    return MapSystemError(errno);
  }
  return Status();
}

StatusOr<int> SocketOp::accept(int fd, sockaddr* addr, socklen_t* addrlen) {
  int accepted_fd = TEMP_FAILURE_RETRY(::accept(fd, addr, addrlen));
  if (accepted_fd == -1) {
    return StatusOr<int>(MapSocketAcceptError(errno));
  }
  return StatusOr<int>(accepted_fd);
}

StatusOr<int> SocketOp::recv(int fd, void* buf, size_t count, int flags) {
  int ret = TEMP_FAILURE_RETRY(::recv(fd, buf, count, flags));
  if (ret == -1) {
    return StatusOr<int>(MapSystemError(errno));
  }
  return StatusOr<int>(ret);
}

Status SocketOp::shutdown(int fd, int how) {
  if (::shutdown(fd, how) == -1) {
    return MapSystemError(errno);
  }
  return Status();
}

Status SocketOp::get_local_name(int fd, sockaddr* addr, socklen_t* addrlen) {
  if (::getsockname(fd, addr, addrlen) == -1) {
    return MapSystemError(errno);
  }
  return Status();
}

Status SocketOp::get_peer_name(int fd, sockaddr* addr, socklen_t* addrlen) {
  if (::getpeername(fd, addr, addrlen) == -1) {
    return MapSystemError(errno);
  }
  return Status();
}

Status SocketOp::set_nodelay(int fd, bool enable) {
  int on = enable ? 1 : 0;
  if (::setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &on, sizeof(on)) == -1) {
    return MapSystemError(errno);
  }
  return Status();
}

Status SocketOp::set_reuse_addr(int fd, bool enable) {
  int on = enable ? 1 : 0;
  if (::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) == -1) {
    return MapSystemError(errno);
  }
  return Status();
}

Status SocketOp::set_keep_alive(int fd, bool enable, int delay) {
  int on = enable ? 1 : 0;
  if (::setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &on, sizeof(on)) == -1) {
    return MapSystemError(errno);
  }

  // setting the keepalive interval varies by platform
  if (::setsockopt(fd, SOL_TCP, TCP_KEEPIDLE, &delay, sizeof(delay)) == -1) {
    return MapSystemError(errno);
  }

  // set seconds between TCP keep alvies
  if (::setsockopt(fd, SOL_TCP, TCP_KEEPINTVL, &delay, sizeof(delay)) == -1) {
    return MapSystemError(errno);
  }

  return Status();
}

Status SocketOp::set_receive_buffer_size(int fd, int size) {
  if (::setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &size, sizeof(size)) == -1) {
    return MapSystemError(errno);
  }
  return Status();
}

Status SocketOp::set_send_buffer_size(int fd, int size) {
  if (::setsockopt(fd, SOL_SOCKET, SO_SNDBUF, &size, sizeof(size)) == -1) {
    return MapSystemError(errno);
  }
  return Status();
}

}  // namespace iomgr
