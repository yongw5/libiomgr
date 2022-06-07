#include "io/io_manager.h"

#include <glog/logging.h>

#include "threading/task_handle.h"
#include "threading/task_runner.h"
#include "threading/thread.h"
#include "timer/timer_manager.h"

namespace iomgr {
const int kMaxPollEvents = 100;

class IOManager::PollThread : public Thread {
 public:
  explicit PollThread(IOManager* iomgr) : iomgr_(CHECK_NOTNULL(iomgr)) {}

 private:
  void ThreadEntry() override { iomgr_->Run(); }

  IOManager* iomgr_;
};

class WakeupWatcher : public IOWatcher {
 public:
  void OnFileReadable(int fd) override {
    Status status;
    uint64_t value;
    do {
      status = FileOp::eventfd_read(fd, &value);
    } while (status.ok());
  }
  void OnFileWritable(int fd) override { DCHECK(false) << "NOTREACHED"; }

  ~WakeupWatcher() = default;
};

IOManager* IOManager::Get() {
  static IOManager s_iomgr;
  return &s_iomgr;
}

IOManager::IOManager()
    : mutex_(),
      stopped_(false),
      poller_(kMaxPollEvents),
      wakeup_fd_(-1),
      fd_controllers_(),
      poll_thread_(),
      wakeup_controller_() {
  StatusOr<int> eventfd = FileOp::eventfd(0, /* non_blocking */ true);
  DCHECK(eventfd.ok());
  wakeup_fd_.reset(eventfd.value());
  DCHECK(WatchFileDescriptor(wakeup_fd_, IOWatcher::kWatchRead,
                             new WakeupWatcher, &wakeup_controller_));
  poll_thread_.reset(new PollThread(this));
  DCHECK(poll_thread_);
  poll_thread_->StartThread();
}

IOManager::~IOManager() {
  {
    MutexLock lock(&mutex_);
    stopped_ = true;
  }
  Wakeup();
  poll_thread_->StopThread();
  delete dynamic_cast<WakeupWatcher*>(wakeup_controller_.watcher());
}

bool IOManager::WatchFileDescriptor(int fd, int mode, IOWatcher* watcher,
                                    IOWatcher::Controller* controller) {
  DCHECK_GE(fd, 0);
  DCHECK(watcher);
  DCHECK(controller);
  DCHECK(mode == IOWatcher::kWatchRead || mode == IOWatcher::kWatchWrite ||
         mode == IOWatcher::kWatchReadWrite);

  MutexLock lock(&mutex_);
  if (controller->fd() != -1 && controller->fd() != fd) {
    LOG(ERROR) << "Cannot use the same IOWatchController on two different FDs";
    return false;
  }

  StopWatchingFileDescriptorNoLock(controller);

  Status status;
  auto rv = fd_controllers_.insert(FDAndControllers(fd));
  FDAndControllers* fd_ctrl = const_cast<FDAndControllers*>(&*(rv.first));
  if (fd_ctrl->mode == 0) {
    if (!(status = poller_.AddFd(fd, mode, fd_ctrl)).ok()) {
      DLOG(ERROR) << "Failed to add fd into IOPoller" << status.ToString();
      return false;
    }
  } else {
    if (!(status = poller_.UpdateFd(fd, mode | fd_ctrl->mode, fd_ctrl)).ok()) {
      DLOG(ERROR) << "Failed to update fd in IOPoller" << status.ToString();
      return false;
    }
  }

  controller->fd_ = fd;
  controller->mode_ = mode;
  controller->watcher_ = watcher;
  fd_ctrl->mode |= mode;
  fd_ctrl->controllers.push_back(controller);
  return true;
}

bool IOManager::StopWatchingFileDescriptor(IOWatcher::Controller* controller) {
  MutexLock lock(&mutex_);
  return StopWatchingFileDescriptorNoLock(controller);
}

void IOManager::Wakeup() {
  uint64_t value = 2;
  if (!FileOp::eventfd_write(wakeup_fd_, value).ok()) {
    LOG(ERROR) << "Failed to wake up iomanger";
  }
}

void IOManager::Run() {
  while (true) {
    Time::Delta timeout = TimerManager::Get()->TimerCheck();
    if (timeout.IsInfinite()) {
      timeout = Time::Delta::FromMilliseconds(-1);
    } else if (timeout < Time::Delta::Zero()) {
      timeout = Time::Delta::Zero();
    } else if(timeout < Time::Delta::FromMilliseconds(1)) {
      timeout = Time::Delta::FromMilliseconds(1);
    }

    std::vector<IOEvent> io_events;
    Status status = poller_.Poll(timeout, &io_events);
    if (!(status.ok() || status.IsTimeout())) {
      LOG(ERROR) << "Failed to poll: " << status.ToString();
      return;
    }
    TaskRunner* runner = TaskRunner::Get();
    for (auto& event : io_events) {
      FDAndControllers* fd_ctrl =
          reinterpret_cast<FDAndControllers*>(event.data);
      DCHECK(fd_ctrl);
      MutexLock lock(&mutex_);
      for (auto ctrl : fd_ctrl->controllers) {
        if (event.ready & ctrl->mode()) {
          ctrl->task_.reset(new TaskHandle(runner->PostTask(
              std::bind(&IOManager::HandleIO, ctrl->fd(), ctrl->watcher(),
                        event.ready & ctrl->mode()))));
        }
      }
    }
    MutexLock lock(&mutex_);
    if (stopped_) {
      return;
    }
  }
}

void IOManager::HandleIO(int fd, IOWatcher* watcher, int ready) {
  DCHECK_NE(-1, fd);
  DCHECK(watcher);
  DCHECK_GT(ready, 0);

  if (ready & IOWatcher::kWatchWrite) {
    watcher->OnFileWritable(fd);
  }
  if (ready & IOWatcher::kWatchRead) {
    watcher->OnFileReadable(fd);
  }
}

bool IOManager::StopWatchingFileDescriptorNoLock(
    IOWatcher::Controller* controller) {
  DCHECK(controller);

  int fd = controller->fd();
  int mode = controller->mode();
  std::unique_ptr<TaskHandle> task = std::move(controller->task_);

  if (fd == -1) {
    return true;
  }
  if (task) {
    task->CancelTask();
  }
  auto fd_i = fd_controllers_.find(FDAndControllers(fd));
  DCHECK(fd_i != fd_controllers_.end());
  FDAndControllers* fd_ctrl = const_cast<FDAndControllers*>(&*fd_i);
  DCHECK(fd_ctrl->mode);
  DCHECK(!fd_ctrl->controllers.empty());
  for (auto it = fd_ctrl->controllers.begin(); it != fd_ctrl->controllers.end();
       ++it) {
    if (*it == controller) {
      fd_ctrl->controllers.erase(it);
      break;
    }
  }

  fd_ctrl->mode &= ~mode;
  if (fd_ctrl->mode == 0) {
    if (!poller_.RemoveFd(fd).ok()) {
      return false;
    }
  } else {
    if (!poller_.UpdateFd(fd, fd_ctrl->mode, fd_ctrl).ok()) {
      return false;
    }
  }
  if (task) {
    task->WaitIfRunning();
  }
  controller->reset();

  return true;
}

}  // namespace iomgr
