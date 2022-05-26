#ifndef LIBIOIOMGR_UTIL_SCOPED_FD_H_
#define LIBIOIOMGR_UTIL_SCOPED_FD_H_

#include <unistd.h>

#include "util/file_op.h"

namespace iomgr {

class ScopedFD {
 public:
  ScopedFD() : fd_(-1) {}
  explicit ScopedFD(int fd) : fd_(fd) {}

  ScopedFD(const ScopedFD&) = delete;
  ScopedFD& operator=(const ScopedFD&) = delete;

  ~ScopedFD() {
    if (-1 != fd_) {
      Status status = FileOp::close(fd_);
      if (!status.ok()) {
        LOG(ERROR) << "Failed to close(" << fd_ << ") " << status.ToString();
      }
      fd_ = -1;
    }
  }

  void reset(int fd) {
    if (-1 != fd_) {
      ::close(fd_);
    }
    fd_ = fd;
  }

  void reset() { reset(-1); }

  int release() {
    int prev = fd_;
    fd_ = -1;
    return prev;
  }

  operator int() const { return fd_; }

 private:
  int fd_;
};

}  // namespace iomgr

#endif  // IOMGR_UTIL_SCOPED_FD_H_