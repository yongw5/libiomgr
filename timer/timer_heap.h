#ifndef LIBIOMGR_TIMER_TIMER_HEAP_H_
#define LIBIOMGR_TIMER_TIMER_HEAP_H_

#include <vector>

#include "iomgr/time.h"

namespace iomgr {

class Timer;

class TimerHeap {
 public:
  TimerHeap() = default;
  ~TimerHeap() = default;
  TimerHeap(const TimerHeap&) = delete;
  TimerHeap& operator=(const TimerHeap&) = delete;

  // Return true iff the new timers is the first timer in the heap
  bool Add(Timer* timer);
  void Remove(Timer* timer);
  Timer* Top();
  void Pop();
  size_t timer_count() const { return timers_.size(); }
  bool empty() const { return timers_.empty(); }

  // This is for testing only
  static void ResetDeadline(Timer* timer, Time deadline);
  
  bool Contains(Timer* timer);
  bool CheckValid();

 private:
  // Adjusts a heap so as to move element at position |i| closer to the root.
  // This functor is called each time immediately after modifying a value in the
  // underlying container, with the offset of the modified element as its
  // argument.
  void AdjustUpwards(uint32_t i);
  // Adjusts a heap so as to move a element at position |i| farther away from
  // the root.
  void AdjustDownwards(uint32_t i);

  void MaybeShrink();

  std::vector<Timer*> timers_;
};

}  // namespace iomgr

#endif // LIBIOMGR_UTIL_TIMER_HEAP_H_