#ifndef LIBIOMGR_INCLUDE_TIMER_H_
#define LIBIOMGR_INCLUDE_TIMER_H_

#include <functional>
#include <memory>

#include "iomgr/export.h"
#include "iomgr/time.h"

namespace iomgr {

class TaskHandle;

class IOMGR_EXPORT Timer {
 public:
  class IOMGR_EXPORT Controller;

  Timer(const Timer&) = delete;
  Timer& operator=(const Timer&) = delete;

  static void Start(Time::Delta delay, std::function<void()> closure,
                    Timer::Controller* controller);

  Time deadline() const { return deadline_; }
  bool pending() const { return pending_; }

 private:
  friend class TimerTest;
  friend class Controller;
  friend class TimerHeap;
  friend class TimerManager;

  Timer();
  ~Timer();

  Time deadline_;
  bool pending_;
  uint32_t heap_index_;
  std::function<void()> closure_;
  Controller* controller_;
  struct {
    Timer* le_next;
    Timer** le_prev;
  } entry_;
};

class IOMGR_EXPORT Timer::Controller {
 public:
  Controller();
  ~Controller();

  Controller(const Controller&) = delete;
  Controller& operator=(const Controller&) = delete;

  void Cancel();

  Time deadline() const { return timer_.deadline(); }

 private:
  friend class TimerManager;

  Timer* timer() { return &timer_; }

  Timer timer_;
  std::unique_ptr<TaskHandle> scheduled_;
};

}  // namespace iomgr

#endif  // LIBIOMGR_INCLUDE_TIMER_H_