#ifndef LIBIOMGR_UTIL_FILE_OP_H_
#define LIBIOMGR_UTIL_FILE_OP_H_

#include "iomgr/status.h"
#include "iomgr/statusor.h"

namespace iomgr {

class FileOp {
 public:
  static StatusOr<int> eventfd(int initval, bool non_blocking);
  static Status eventfd_read(int fd, uint64_t* value);
  static Status eventfd_write(int fd, uint64_t value);
  static StatusOr<int> epoll();
  static Status pipe(int fds[2], bool non_blocking);
  static Status set_non_blocking(int fd);
  static Status set_close_exec(int fd);
  static StatusOr<int> read(int fd, void* buf, size_t count);
  static StatusOr<int> write(int fd, const void* buf, size_t count);
  static Status close(int fd);
};

}  // namespace iomgr

#endif  // LIBIOMGR_UTIL_FILE_OP_H_