#ifndef LIBIOMGR_THREADING_TASK_RUNNER_TASK_H_
#define LIBIOMGR_THREADING_TASK_RUNNER_TASK_H_

#include <atomic>
#include <functional>

#include "iomgr/ref_counted.h"
#include "threading/task_runner.h"
#include "threading/thread.h"
#include "util/notification.h"

namespace iomgr {

class TaskRunner::Task : public TaskHandle::Delegate {
 public:
  explicit Task(std::function<void()> functor);
  virtual ~Task() override;

  void Run();
  void CancelTask() override;
  void WaitIfRunning() override;

 protected:
  friend class TaskRunnerTest;

  enum TaskState {
    kPending,
    kRunning,
    kCanceled,
    kCompleted,
  };

  std::function<void()> functor_;
  std::atomic<TaskState> task_state_;
  Notification task_completed_;
  std::atomic<Thread::Id> task_runner_;
};

}  // namespace iomgr

#endif  // LIBIOMGR_THREADING_TASK_RUNNER_TASK_H_