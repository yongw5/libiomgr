#ifndef LIBIOMGR_TIMER_TEST_TIMER_TEST_H_
#define LIBIOMGR_TIMER_TEST_TIMER_TEST_H_

#include "iomgr/time.h"
#include "iomgr/timer.h"

namespace iomgr {

class TimerTest {
 public:
  TimerTest() : timer_(), inserted_(false) {}
  TimerTest(Time deadline) : timer_(), inserted_(false) {
    timer_.deadline_ = deadline;
  }

  Timer* timer() { return &timer_; }
  Time deadline() const { return timer_.deadline(); }
  bool inserted() const { return inserted_; }
  void set_inserted(bool inserted) { inserted_ = inserted; }

 private:
  Timer timer_;
  bool inserted_;
};

}  // namespace iomgr

#endif  // LIBIOMGR_TIMER_TEST_TIMER_TEST_H_