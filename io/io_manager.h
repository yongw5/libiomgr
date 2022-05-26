#ifndef LIBIOMGR_IO_IO_MANAGER_H_
#define LIBIOMGR_IO_IO_MANAGER_H_

#include <set>
#include <vector>

#include "io/io_poller.h"
#include "iomgr/io_watcher.h"
#include "util/scoped_fd.h"
#include "util/sync.h"

namespace iomgr {

class IOManager {
 public:
  static IOManager* Get();

  bool WatchFileDescriptor(int fd, int mode, IOWatcher* watcher,
                           IOWatcher::Controller* controller);
  bool StopWatchingFileDescriptor(IOWatcher::Controller* controller);
  void Wakeup();

 private:
  friend class IOManagerTest;

  struct FDAndControllers {
    FDAndControllers() : fd(-1), mode(0), controllers() {}
    explicit FDAndControllers(int fd) : fd(fd), mode(0), controllers() {}

    int fd;
    int mode;
    std::vector<IOWatcher::Controller*> controllers;
  };

  struct FDAndControllersCmp {
    bool operator()(const FDAndControllers& lhs,
                    const FDAndControllers& rhs) const {
      return lhs.fd < rhs.fd;
    }
  };

  using FDToControllers = std::set<FDAndControllers, FDAndControllersCmp>;

  class PollThread;

  IOManager();
  ~IOManager();

  void Run();
  static void HandleIO(int fd, IOWatcher* watcher, int ready);
  bool StopWatchingFileDescriptorNoLock(IOWatcher::Controller* controller);

  Mutex mutex_;
  bool stopped_;
  IOPoller poller_;
  ScopedFD wakeup_fd_;
  FDToControllers fd_controllers_;
  std::unique_ptr<PollThread> poll_thread_;
  IOWatcher::Controller wakeup_controller_;
};

}  // namespace iomgr

#endif  // LIBIOMGR_IO_IO_MANAGER_H_