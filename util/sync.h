#ifndef LIBIOMGR_UITL_SYNC_H_
#define LIBIOMGR_UITL_SYNC_H_

#include <glog/logging.h>

#include <condition_variable>
#include <mutex>

namespace iomgr {

class CondVar;

// Thinly wraps std::mutex
class Mutex {
 public:
  Mutex() = default;
  ~Mutex() = default;

  Mutex(const Mutex&) = delete;
  Mutex& operator=(const Mutex&) = delete;

  void Lock() { mu_.lock(); }
  void Unlock() { mu_.unlock(); }

 private:
  friend class CondVar;
  std::mutex mu_;
};

// Thinly wraps std::condition_variable.
class CondVar {
 public:
  explicit CondVar(Mutex* mu) : mu_(CHECK_NOTNULL(mu)) {}
  ~CondVar() = default;

  CondVar(const CondVar&) = delete;
  CondVar& operator=(const CondVar&) = delete;

  void Wait() {
    std::unique_lock<std::mutex> lock(mu_->mu_, std::adopt_lock);
    cv_.wait(lock);
    lock.release();
  }
  void Signal() { cv_.notify_one(); }
  void SignalAll() { cv_.notify_all(); }

 private:
  std::condition_variable cv_;
  Mutex* const mu_;
};

// Helper class that locks a mutex on construction and unlocks the mutex when
// the destructor of the MutexLock object is invoked.
//
// Typical usage:
//   void MyClass : MyMethod() {
//     MutexLock l(&mu)); // mu_ is an instance variable
//     ...some complex code, possible with multiple return paths ...
//}
class MutexLock {
 public:
  explicit MutexLock(Mutex* mu) : mu_(CHECK_NOTNULL(mu)) { this->mu_->Lock(); }
  ~MutexLock() { this->mu_->Unlock(); }

  MutexLock(const MutexLock&) = delete;
  MutexLock& operator=(const MutexLock&) = delete;

 private:
  Mutex* mu_;
};

}  // namespace iomgr

#endif  // LIBIOMGR_UITL_SYNC_H_