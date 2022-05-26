#include "threading/task_runner.h"

#include <functional>

#include "threading/task_runner_task.h"
#include "threading/thread.h"

namespace iomgr {

const int kNumThread = 4;

class TaskRunner::Worker : public Thread {
 public:
  explicit Worker(std::function<void()> runnable)
      : runnable_(std::move(runnable)) {}
  ~Worker() = default;

 private:
  void ThreadEntry() override {
    if (runnable_) {
      runnable_();
    }
  }

  std::function<void()> runnable_;
};

TaskRunner::TaskRunner(int num_threads)
    : mutex_(),
      stop_triggered_(false),
      queue_cond_var_(&mutex_),
      tasks_(),
      workers_() {
  for (int i = 0; i < num_threads; ++i) {
    Worker* worker = new Worker(std::bind(&TaskRunner::RunTasks, this));
    workers_.push_back(worker);
    worker->StartThread();
  }
}

TaskRunner* TaskRunner::Get() {
  static TaskRunner s_task_runner(kNumThread);
  return &s_task_runner;
}

TaskRunner::~TaskRunner() {
  {
    MutexLock lock(&mutex_);
    stop_triggered_ = true;
    // Pushing empty task to every thread (and notify them), to have them
    // wakeup.
    for (int i = 0; i < workers_.size(); ++i) {
      tasks_.push(MakeRefCounted<Task>(std::function<void()>()));
    }
    queue_cond_var_.SignalAll();
  }

  for (int i = 0; i < workers_.size(); ++i) {
    workers_[i]->StopThread();
    delete workers_[i];
  }

  // stop_triggered_ is set to true, no more task will be post.
  while (!tasks_.empty()) {
    tasks_.front()->CancelTask();
    tasks_.pop();
  }
}

TaskHandle TaskRunner::PostTask(std::function<void()> functor) {
  MutexLock lock(&mutex_);
  if (stop_triggered_) {
    return TaskHandle();
  }
  bool empty = tasks_.empty();
  RefPtr<Task> task(new Task(functor));
  tasks_.push(task);
  if (empty) {
    queue_cond_var_.Signal();
  }

  return TaskHandle(task);
}

void TaskRunner::RunTasks() {
  bool stop_triggered = false;
  while (true) {
    RefPtr<Task> task;
    {
      MutexLock lock(&mutex_);
      while (tasks_.empty()) {
        queue_cond_var_.Wait();
      }
      if (!(stop_triggered = stop_triggered_)) {
        task = tasks_.front();
        tasks_.pop();
      }
    }
    if (stop_triggered) {
      return;
    }
    task->Run();
  }
}

void TaskRunner::RunTaskForTEST() {
  while (!tasks_.empty()) {
    RefPtr<Task> task;
    task = tasks_.front();
    tasks_.pop();

    task->Run();
  }
}

}  // namespace iomgr
