#ifndef LIBIOMGR_IO_IO_POLLER_H_
#define LIBIOMGR_IO_IO_POLLER_H_

#include <functional>
#include <vector>

#include "iomgr/status.h"
#include "iomgr/time.h"
#include "util/scoped_fd.h"

namespace iomgr {

struct IOEvent {
  int ready;
  void* data;
};

class IOPoller {
 public:
  explicit IOPoller(int max_poll_size);
  ~IOPoller();
  IOPoller(const IOPoller&) = delete;
  IOPoller& operator=(const IOPoller&) = delete;

  Status AddFd(int fd, int mode, void* data);
  Status UpdateFd(int fd, int mode, void* data);
  Status RemoveFd(int fd);
  Status Poll(Time::Delta timeout, std::vector<IOEvent>* io_events);

 private:
  Status InvokeControl(int op, int fd, int mode, void* file_ctx) const;

  ScopedFD epoll_fd_;
  int max_poll_size_;
};

}  // namespace iomgr

#endif  // LIBIOMGR_IO_IO_POLLER_H_