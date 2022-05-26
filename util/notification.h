#ifndef LIBIOMGR_UTIL_NOTIFICATION_H_
#define LIBIOMGR_UTIL_NOTIFICATION_H_

#include "util/sync.h"

namespace iomgr {

class Notification {
 public:
  Notification() : mutex_(), completed_(&mutex_), notified_(false) {}

  Notification(const Notification&) = delete;
  Notification& operator=(const Notification&) = delete;

  void Notify();
  bool HasBeenNotified() const;
  void WaitForNotification();

 private:
  mutable Mutex mutex_;
  CondVar completed_;
  bool notified_;
};

}  // namespace iomgr

#endif  // LIBIOMGR_UTIL_NOTIFICATION_H_