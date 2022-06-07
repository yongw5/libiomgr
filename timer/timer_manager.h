#ifndef LIBIOMGR_TIMER_TIMER_MANAGER_H_
#define LIBIOMGR_TIMER_TIMER_MANAGER_H_

#include <sys/queue.h>

#include <atomic>
#include <vector>

#include "iomgr/timer.h"
#include "timer/timer_heap.h"
#include "util/averaged_stats.h"
#include "util/sync.h"

namespace iomgr {

class TimerManager {
 public:
  static TimerManager* Get();

  TimerManager();
  ~TimerManager();

  TimerManager(const TimerManager&) = delete;
  TimerManager& operator=(const TimerManager&) = delete;

  void TimerInit(Time::Delta delay, Closure closure,
                 Timer::Controller* controller);
  void TimerCancel(Timer::Controller* controller);
  // return next deadline or Time::Infinite() if there is no timer;
  Time::Delta TimerCheck();

 private:
  friend class TimerManagerTest;

  // TimerShard contains a |heap| and a |list| of timers. All timers with
  // deadlines earlier than |heap_capacity| are maintained in the heap and
  // others are maintained in the list (unordered). This helps to keep the
  // nunber of elements in the heap low.
  //
  // The |heap_capacity| gets recomputed periodically based on the timer
  // stats maintained in |stats| and the relevant timers are then moved from the
  // |list| to |heap|
  struct TimerShard {
    TimerShard();
    TimerShard(const TimerShard&) = delete;
    TimerShard& operator=(const TimerShard&) = delete;

    Time ComputeMinDeadline();

    Mutex mutex;
    AveragedStats stats;
    // All and only timers with deadlines < this will be in the heap
    Time heap_capacity;
    // The deadline of next timer due in the shard
    Time min_deadline;
    // index of this TimerShard in the shard_queue
    uint32_t shard_queue_index;
    // This holds all timers with deadlines < heap_capacity.
    TimerHeap urgent_timers;
    // This holds timers whose deadline is >= heap_capacity.
    LIST_HEAD(LessUrgentTimer, Timer) less_urgent_timers;
  };

  void SwapAdjacentShardsInQueue(uint32_t first);
  void OnDeadlineChanged(TimerShard* shard);
  // Reblance the timer shard by computing a new |shard->heap_capacity| and
  // moving all relavant timers in |shard->list| (i.e timers with deadlines
  // earlier than |shard->heap_capacity|) into |shard->heap|.
  // Returns true if |shard->heap| has at least ONE element.
  // REQUIRES: |shard->mutex| locked.
  static bool RefillHeap(TimerShard* shard, Time now);
  // Pops the next non-cancelled tiemr with deadline <= |now| from the queue, or
  // returns NULL if there isn't one. REQUIRES: |shard->mutex| locked
  static Timer* PopOne(TimerShard* shard, Time now);
  // REQUIRED: |shard->mutex| unlocked
  static size_t PopTimers(TimerShard* shard, Time now, Time* new_min_deadline);

  // Protects |shard_queue_| and |min_deadline_|
  Mutex mutex_;
  // Array of timer shards. Whenever a timer is added, its address is hashed to
  // select the timer shard to add the timer to.
  std::vector<TimerShard> shards_;
  // Maintains a sorted list of timer shards (sorted by their |min_deadline|,
  // i.e the deadline of the next timer in each shard).
  std::vector<TimerShard*> shard_queue_;
};

}  // namespace iomgr

#endif  // LIBIOMGR_TIMER_TIMER_MANAGER_H_