#include "iomgr/io_watcher.h"

#include <glog/logging.h>

#include "io/io_manager.h"
#include "threading/task_handle.h"

namespace iomgr {

bool IOWatcher::WatchFileDescriptor(int fd, int mode, IOWatcher* watcher,
                                    Controller* controller) {
  DCHECK_GE(fd, 0);
  DCHECK(watcher);
  DCHECK(controller);
  DCHECK(mode == IOWatcher::kWatchRead || mode == IOWatcher::kWatchWrite ||
         mode == IOWatcher::kWatchReadWrite);

  return IOManager::Get()->WatchFileDescriptor(fd, mode, watcher, controller);
}

/// IOWatcher
IOWatcher::~IOWatcher() = default;

// IOWatcher::Controller
IOWatcher::Controller::Controller()
    : fd_(-1), mode_(0), watcher_(nullptr), task_(nullptr) {}

IOWatcher::Controller::~Controller() { DCHECK(StopWatching()); }

void IOWatcher::Controller::reset() {
  fd_ = -1;
  mode_ = 0;
  watcher_ = nullptr;
  task_.reset();
}

bool IOWatcher::Controller::StopWatching() {
  return IOManager::Get()->StopWatchingFileDescriptor(this);
}

}  // namespace iomgr
