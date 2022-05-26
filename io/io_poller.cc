#include "io/io_poller.h"

#include <glog/logging.h>
#include <sys/epoll.h>
#include <unistd.h>

#include "iomgr/io_watcher.h"
#include "util/file_op.h"
#include "util/os_error.h"

namespace iomgr {

IOPoller::IOPoller(int max_poll_size)
    : epoll_fd_(-1), max_poll_size_(max_poll_size) {
  StatusOr<int> ret = FileOp::epoll();
  if (!ret.ok()) {
    LOG(ERROR) << "Failed to create epoll: " << ret.status().ToString();
  } else {
    epoll_fd_.reset(ret.value());
  }
}

IOPoller::~IOPoller() {}

Status IOPoller::AddFd(int fd, int mode, void* data) {
  return InvokeControl(EPOLL_CTL_ADD, fd, mode, data);
}

Status IOPoller::UpdateFd(int fd, int mode, void* data) {
  return InvokeControl(EPOLL_CTL_MOD, fd, mode, data);
}

Status IOPoller::RemoveFd(int fd) {
  return InvokeControl(EPOLL_CTL_DEL, fd, 0, nullptr);
}

Status IOPoller::Poll(Time::Delta timeout, std::vector<IOEvent>* io_events) {
  DCHECK(io_events);

  Time::Delta remaining_time = Time::Delta::FromMilliseconds(-1);
  if (timeout >= Time::Delta::Zero()) {
    remaining_time = timeout;
  }

  int rc = -1;
  io_events->clear();
  std::vector<struct epoll_event> events(max_poll_size_);
  do {
    Time start = Time::Now();
    rc = epoll_wait(epoll_fd_, events.data(), max_poll_size_,
                    remaining_time.ToMilliseconds());
    if (rc < 0 && errno == EINTR) {
      Time end = Time::Now();
      Time::Delta waited = end - start;
      if (waited < remaining_time) {
        remaining_time = remaining_time - waited;
      } else {
        return Status::Timeout("epoll_wait timeout");
      }
    }
  } while (rc < 0 && errno == EINTR);

  if (rc < 0) {
    return MapSystemError(errno);
  } else {
    for (int i = 0; i < rc; ++i) {
      IOEvent io_event = {.ready = 0, .data = events[i].data.ptr};
      if (events[i].events & (EPOLLIN | EPOLLERR | EPOLLHUP | EPOLLPRI)) {
        io_event.ready |= IOWatcher::kWatchRead;
      }
      if (events[i].events & (EPOLLOUT | EPOLLERR | EPOLLHUP | EPOLLPRI)) {
        io_event.ready |= IOWatcher::kWatchWrite;
      }
      if (io_event.ready) {
        io_events->push_back(io_event);
      }
    }
  }

  return Status();
}

Status IOPoller::InvokeControl(int op, int fd, int mode, void* file_ctx) const {
  int events = EPOLLET;
  if (mode & IOWatcher::kWatchRead) {
    events |= EPOLLIN;
  }
  if (mode & IOWatcher::kWatchWrite) {
    events |= EPOLLOUT;
  }

  struct epoll_event event;
  memset(&event, 0, sizeof(event));
  event.events = events;
  event.data.ptr = file_ctx;
  if (::epoll_ctl(epoll_fd_, op, fd, &event) == -1) {
    return MapSystemError(errno);
  }
  return Status();
}

}  // namespace iomgr