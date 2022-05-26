#ifndef LIBIOMGR_THREADING_TASK_RUNNER_H_
#define LIBIOMGR_THREADING_TASK_RUNNER_H_

#include <functional>
#include <queue>

#include "iomgr/ref_counted.h"
#include "threading/task_handle.h"
#include "util/sync.h"

namespace iomgr {

class TaskRunner {
 public:
  static TaskRunner* Get();

  TaskRunner(const TaskRunner&) = delete;
  TaskRunner& operator=(const TaskRunner&) = delete;

  TaskHandle PostTask(std::function<void()> functor);

 private:
  friend class TaskRunnerTest;

  class Worker;
  class Task;

  explicit TaskRunner(int num_threads);
  ~TaskRunner();

  void RunTasks();
  void RunTaskForTEST();

  Mutex mutex_;
  CondVar queue_cond_var_;
  bool stop_triggered_;
  std::queue<RefPtr<Task>> tasks_;
  std::vector<Worker*> workers_;
};

}  // namespace iomgr

#endif  // LIBIOMGR_THREADING_TASK_RUNNER_H_