#include "util/notification.h"

#include <glog/logging.h>

namespace iomgr {

void Notification::Notify() {
  MutexLock lock(&mutex_);
  DCHECK(!notified_);
  notified_ = true;
  completed_.SignalAll();
}

bool Notification::HasBeenNotified() const {
  MutexLock lock(&mutex_);
  return notified_;
}

void Notification::WaitForNotification() {
  MutexLock lock(&mutex_);
  while (!notified_) {
    completed_.Wait();
  }
}

}  // namespace iomgr
