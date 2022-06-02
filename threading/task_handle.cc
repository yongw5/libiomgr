#include "threading/task_handle.h"

#include "glog/logging.h"

namespace iomgr {

TaskHandle::TaskHandle() = default;

TaskHandle::TaskHandle(RefPtr<Delegate> delegate)
    : delegate_(std::move(delegate)) {}

TaskHandle::TaskHandle(TaskHandle&& other) = default;

TaskHandle& TaskHandle::operator=(TaskHandle&& other) {
  delegate_ = std::move(other.delegate_);
  return *this;
}

TaskHandle::~TaskHandle() = default;

void TaskHandle::CancelTask() {
  if (delegate_) {
    delegate_->CancelTask();
  }
}

void TaskHandle::WaitIfRunning() {
  if (delegate_) {
    delegate_->WaitIfRunning();
  }
}

}  // namespace iomgr