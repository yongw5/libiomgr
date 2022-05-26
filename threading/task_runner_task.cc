#include "threading/task_runner_task.h"

#include <glog/logging.h>

namespace iomgr {

TaskRunner::Task::Task(std::function<void()> functor)
    : functor_(std::move(functor)),
      task_state_(kPending),
      task_completed_(),
      task_runner_(Thread::invalid_id) {}

TaskRunner::Task::~Task() = default;

void TaskRunner::Task::Run() {
  TaskState expected = kPending;
  if (!task_state_.compare_exchange_strong(expected, kRunning)) {
    return;
  }
  task_runner_.store(CurrentThread::get_id());
  if (functor_) {
    functor_();
  }
  task_state_.store(kCompleted);
  task_completed_.Notify();
  task_runner_.store(Thread::invalid_id);
}

void TaskRunner::Task::CancelTask() {
  while (true) {
    TaskState state = task_state_.load();
    switch (state) {
      case kPending:
      case kRunning:
        if (task_state_.compare_exchange_strong(state, kCanceled)) {
          return;
        }
        break;
      case kCompleted:
        return;
      default:
        DCHECK(0) << "No reached";
        break;
    }
  }
}

void TaskRunner::Task::WaitIfRunning() {
  if (task_runner_.load() != CurrentThread::get_id()) {
    if (kRunning == task_state_.load()) {
      task_completed_.WaitForNotification();
    }
  }
}

}  // namespace iomgr
