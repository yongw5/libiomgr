#ifndef LIBIOMGR_THREADING_THREAD_H_
#define LIBIOMGR_THREADING_THREAD_H_

#include <memory>
#include <thread>

#include "iomgr/time.h"

namespace iomgr {

class Thread {
 public:
  using Id = std::thread::id;
  static const Id invalid_id;

  Thread() : thread_() {}
  virtual ~Thread();

  Thread(const Thread&) = delete;
  Thread& operator=(const Thread&) = delete;

  void StartThread();
  void StopThread();
  bool started() const;
  Id get_id() const;

 protected:
  // Derived class must implement this function
  virtual void ThreadEntry() = 0;
  virtual void TerminateThread() {}
  bool must_stop();

 private:
  std::unique_ptr<std::thread> thread_;
};

class CurrentThread {
 public:
  static void SleepFor(Time::Delta duration);
  static void SleepUntil(Time timepoint);
  static Thread::Id get_id();
};

}  // namespace iomgr

#endif  // LIBIOMGR_THREADING_THREAD_H_