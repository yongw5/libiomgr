#include "util/file_op.h"

#include <error.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <unistd.h>

#include "util/os_error.h"

namespace iomgr {

StatusOr<int> FileOp::eventfd(int initval, bool non_blocking) {
  int flags = EFD_CLOEXEC;
  if (non_blocking) {
    flags |= EFD_NONBLOCK;
  }
  int fd = ::eventfd(initval, flags);
  if (fd != -1) {
    return StatusOr<int>(fd);
  } else {
    return StatusOr<int>(MapSystemError(errno));
  }
}

Status FileOp::eventfd_read(int fd, uint64_t* value) {
  int read = TEMP_FAILURE_RETRY(::eventfd_read(fd, value));
  if (read == -1) {
    return MapSystemError(errno);
  }
  return Status();
}

Status FileOp::eventfd_write(int fd, uint64_t value) {
  int wrote = TEMP_FAILURE_RETRY(::eventfd_write(fd, value));
  if (wrote == -1) {
    return MapSystemError(errno);
  }
  return Status();
}

StatusOr<int> FileOp::epoll() {
  int fd = ::epoll_create1(EPOLL_CLOEXEC);
  if (fd == -1) {
    return StatusOr<int>(MapSystemError(errno));
  }
  return StatusOr<int>(fd);
}

Status FileOp::pipe(int fds[2], bool non_blocking) {
  int flags = O_CLOEXEC;
  if (non_blocking) {
    flags |= O_NONBLOCK;
  }
  if (::pipe2(fds, flags) == -1) {
    return MapSystemError(errno);
  }
  return Status();
}

Status FileOp::set_non_blocking(int fd) {
  const int flags = fcntl(fd, F_GETFL);
  if (flags == -1) {
    return MapSystemError(errno);
  }
  if (flags & O_NONBLOCK) {
    return Status();
  }
  if (::fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) {
    return MapSystemError(errno);
  }
  return Status();
}

Status FileOp::set_close_exec(int fd) {
  const int flags = ::fcntl(fd, F_GETFD);
  if (flags == -1) {
    return MapSystemError(errno);
  }
  if (flags & FD_CLOEXEC) {
    return Status();
  }
  if (::fcntl(fd, F_SETFD, flags | FD_CLOEXEC) == -1) {
    return MapSystemError(errno);
  }

  return Status();
}

StatusOr<int> FileOp::read(int fd, void* buf, size_t count) {
  int read = TEMP_FAILURE_RETRY(::read(fd, buf, count));
  if (read == -1) {
    return StatusOr<int>(MapSystemError(errno));
  }
  return StatusOr<int>(read);
}

StatusOr<int> FileOp::write(int fd, const void* buf, size_t count) {
  int wrote = TEMP_FAILURE_RETRY(::write(fd, const_cast<void*>(buf), count));
  if (wrote == -1) {
    return StatusOr<int>(MapSystemError(errno));
  }
  return StatusOr<int>(wrote);
}

Status FileOp::close(int fd) {
  DCHECK_NE(-1, fd);
  if (TEMP_FAILURE_RETRY(::close(fd)) == -1) {
    return MapSystemError(errno);
  } else {
    return Status();
  }
}

}  // namespace iomgr