#ifndef LIBIOMGR_INCLUDE_IO_WATCHER_H_
#define LIBIOMGR_INCLUDE_IO_WATCHER_H_

#include <memory>

#include "iomgr/export.h"

namespace iomgr {

class TaskHandle;

class IOMGR_EXPORT IOWatcher {
 public:
  class Controller;
  enum {
    kWatchRead = 1 << 0,
    kWatchWrite = 1 << 1,
    kWatchReadWrite = kWatchRead | kWatchWrite,
  };

  static bool WatchFileDescriptor(int fd, int mode, IOWatcher* watcher,
                                  Controller* controller);

  virtual void OnFileReadable(int fd) = 0;
  virtual void OnFileWritable(int fd) = 0;

 protected:
  virtual ~IOWatcher();
};

class IOMGR_EXPORT IOWatcher::Controller {
 public:
  Controller();
  ~Controller();

  Controller(const Controller&) = delete;
  Controller& operator=(const Controller&) = delete;

  bool StopWatching();

 private:
  friend class IOManager;

  int fd() const { return fd_; }
  int mode() const { return mode_; }
  IOWatcher* watcher() const { return watcher_; }
  void reset();

  int fd_;
  int mode_;
  IOWatcher* watcher_;
  std::unique_ptr<TaskHandle> task_;
};

}  // namespace iomgr

#endif  // LIBIOMGR_INCLUDE_IO_WATCHER_H_