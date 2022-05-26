#ifndef LIBIOMGR_TIMER_TIMER_MANAGER_H_
#define LIBIOMGR_TIMER_TIMER_MANAGER_H_

#include <sys/queue.h>

#include <atomic>
#include <vector>

#include "iomgr/timer.h"
#include "timer/timer_heap.h"
#include "util/sync.h"

namespace iomgr {

enum TimerCheckResult {
  kNotChecked,
  kCheckedAndEmpty,
  kTimersFired,
};

class TimerManager {
 public:
  static TimerManager* Get();

  TimerManager();
  ~TimerManager();

  TimerManager(const TimerManager&) = delete;
  TimerManager& operator=(const TimerManager&) = delete;

  void TimerInit(Time::Delta delay, std::function<void()> closure,
                 Timer::Controller* controller);
  void TimerCancel(Timer::Controller* controller);

  TimerCheckResult TimerCheck(Time* next);

 private:
  friend class TimerManagerTest;

  class TimeAveragedStats;
  class TimerShard;

  void SwapAdjacentShardsInQueue(uint32_t first);
  void OnDeadlineChanged(TimerShard* shard);
  // Reblance the timer shard by computing a new |shard->queue_deadline_cap| and
  // moving all relavant timers in |shard->list| (i.e timers with deadlines
  // earlier than |shard->queue_deadline_cap|) into |shard->heap|.
  // Returns true if |shard->heap| has at least ONE element.
  // REQUIRES: |shard->mutex| locked.
  bool RefillHeap(TimerShard* shard, Time now);
  // Pops the next non-cancelled tiemr with deadline <= |now| from the queue, or
  // returns NULL if there isn't one. REQUIRES: |shard->mutex| locked
  Timer* PopOne(TimerShard* shard, Time now);
  // REQUIRED: |shard->mutex| unlocked
  size_t PopTimers(TimerShard* shard, Time now, Time* new_min_deadline);

  // Array of timer shards. Whenever a timer is added, its address is hashed to
  // select the timer shard to add the timer to.
  std::vector<TimerShard> shards_;
  // Maintains a sorted list of timer shards (sorted by their |min_deadline|,
  // i.e the deadline of the next timer in each shard).
  std::vector<TimerShard*> shard_queue_;
  // The deadline of the next timer due across all timer shards
  Time min_deadline_;
  // Protects |shard_queue_| and |min_deadline_|
  Mutex mutex_;
  // Indicated there is a thread which calls TimerCheck.
  std::atomic<bool> in_checking_;
};

class TimerManager::TimeAveragedStats {
 public:
  TimeAveragedStats(double init_avg, double regress_weight,
                    double persistence_factor);
  void AddSample(double value);
  double UpdateAverage();

  double init_avg() const { return init_avg_; }
  double regress_weight() const { return regress_weight_; }
  double persistence_factor() const { return persistence_factor_; }
  double batch_total_value() const { return batch_total_value_; }
  double batch_num_samples() const { return batch_num_samples_; }
  double aggregate_total_weight() const { return aggregate_total_weight_; }
  double aggregate_weighted_avg() const { return aggregate_weighted_avg_; }

 private:
  // The initial average value. This is the reported average until
  // the first UpdateAverage() call. If a positive |regress_weight_| is used,
  // we also regress towards this value on each update.
  double init_avg_;
  // The sample weight of |init_avg_| that is mxied in with each call to
  // UpdateAverage(). If the calls to AddSample() stop, this will cause the
  // average to regress back to the |init_avg_|. This should be non-negative.
  // Set it to 0 to disable the bias. A value of 1 has the effect of adding in
  // 1 bonus sample with value |init_avg_| to each sample period.
  double regress_weight_;
  // This determines the rate of decay of the time-averaging from one period
  // to the next by scaling the |aggreate_total_weight_| of samples from prior
  // periods when combining with the latest period. It should be in the range
  // [0, 1]. A higher value adapts more slowly. With a value of 0.5, if the
  // batches each have k samples, the weighting of the time average will
  // eventually be 1/3 new batch and 2/3 old average.
  double persistence_factor_;
  // the total value of samples since the last UpdateAverage()
  double batch_total_value_;
  // The number of samples since the last UpdateAverage()
  double batch_num_samples_;
  // The time-decayed sum of |batch_num_samples_| over preview batches. This
  // is the "weight" of the old |aggregate_weighted_avg_| when updating the
  // average.
  double aggregate_total_weight_;
  // A time-decayed average of the |total_batch_value_| /
  // |batch_num_samples_|, computed by decaying the samples_in_avg weight in
  // the weighted averages.
  double aggregate_weighted_avg_;
};

// TimerShard contains a |heap| and a |list| of timers. All timers with
// deadlines earlier than |queue_deadline_cap| are maintained in the heap and
// others are maintained in the list (unordered). This helps to keep the
// nunber of elements in the heap low.
//
// The |queue_deadline_cap| gets recomputed periodically based on the timer
// stats maintained in |stats| and the relevant timers are then moved from the
// |list| to |heap|
struct TimerManager::TimerShard {
  TimerShard();
  TimerShard(const TimerShard&) = delete;
  TimerShard& operator=(const TimerShard&) = delete;

  Time ComputeMinDeadline();

  Mutex mutex;
  TimeAveragedStats stats;
  // All and only timers with deadlines < this will be in the heap
  Time queue_deadline_cap;
  // The deadline of next timer due in the shard
  Time min_deadline;
  // index of this TimerShard in the g_shard_queue
  uint32_t shard_queue_index;
  // This holds all timers with deadlines < queue_deadline_cap.
  TimerHeap urgent_timers;
  // This holds timers whose deadline is >= queue_deadline_cap.
  LIST_HEAD(LessUrgentTimer, Timer) less_urgent_timers;
};

}  // namespace iomgr

#endif  // LIBIOMGR_TIMER_TIMER_MANAGER_H_