#include "timer/timer_manager.h"

#include <sys/queue.h>

#include <algorithm>

#include "io/io_manager.h"
#include "iomgr/timer.h"
#include "threading/task_handle.h"
#include "threading/task_runner.h"
#include "util/pointer_hash.h"

#define ADD_DEADLINE_SCALE 0.33
#define MIN_QUEUE_WINDOW_DURATION 0.01
#define MAX_QUEUE_WINDOW_DURATION 1.0

#define NUM_SHARDS 8

namespace iomgr {

const uint32_t kInvalidIndex = 0xffffffffu;

TimerManager* TimerManager::Get() {
  static TimerManager s_timer_manager;
  IOManager::Get();

  return &s_timer_manager;
}

TimerManager::TimerManager()
    : shards_(NUM_SHARDS),
      shard_queue_(NUM_SHARDS),
      min_deadline_(Time::Now()),
      mutex_(),
      in_checking_(false) {
  for (int i = 0; i < NUM_SHARDS; ++i) {
    TimerShard* shard = &shards_[i];
    shard->queue_deadline_cap = min_deadline_;
    shard->shard_queue_index = i;
    shard->min_deadline = shard->ComputeMinDeadline();
    shard_queue_[i] = shard;
  }
}

TimerManager::~TimerManager() = default;

void TimerManager::TimerInit(Time::Delta timeout, std::function<void()> closure,
                             Timer::Controller* controller) {
  DCHECK(controller);

  bool is_first_timer = false;
  Time deadline = Time::Now() + timeout;
  Timer* timer = controller->timer();
  TimerShard* shard = &shards_[(PointerHash<Timer*>{}(timer)) & NUM_SHARDS];
  {
    MutexLock lock(&shard->mutex);
    timer->deadline_ = deadline;
    timer->pending_ = true;
    timer->closure_ = std::move(closure);
    shard->stats.AddSample(static_cast<double>(timeout.ToSeconds()));
    if (deadline < shard->queue_deadline_cap) {
      is_first_timer = shard->urgent_timers.Add(timer);
    } else {
      timer->heap_index_ = kInvalidIndex;
      LIST_INSERT_HEAD(&shard->less_urgent_timers, timer, entry_);
    }
  }
  if (is_first_timer) {
    MutexLock lock(&mutex_);
    if (deadline < shard->min_deadline) {
      Time old_min_deadline = shard_queue_[0]->min_deadline;
      shard->min_deadline = deadline;
      OnDeadlineChanged(shard);
      if (shard->shard_queue_index == 0 && deadline < old_min_deadline) {
        min_deadline_ = deadline;
      }
    }
  }
}

void TimerManager::TimerCancel(Timer::Controller* controller) {
  DCHECK(controller);

  Timer* timer = controller->timer();
  TimerShard* shard = &shards_[(PointerHash<Timer*>{}(timer)) & NUM_SHARDS];
  MutexLock lock(&shard->mutex);
  std::unique_ptr<TaskHandle> task = std::move(controller->scheduled_);
  if (timer->pending_) {
    timer->pending_ = false;
    if (timer->heap_index_ == kInvalidIndex) {
      LIST_REMOVE(timer, entry_);
      timer->entry_.le_prev = nullptr;
    } else {
      shard->urgent_timers.Remove(timer);
    }
  }
}

TimerCheckResult TimerManager::TimerCheck(Time* next) {
  DCHECK(next);

  bool expected = false;
  if (!in_checking_.compare_exchange_strong(expected, true)) {
    return kNotChecked;
  }
  Time now = Time::Now();
  MutexLock lock(&mutex_);
  if (now < min_deadline_) {
    *next = std::min(*next, min_deadline_);
    return kCheckedAndEmpty;
  }

  TimerCheckResult ret = kCheckedAndEmpty;
  while (shard_queue_[0]->min_deadline <= now) {
    Time new_min_deadline = Time::Zero();
    if (PopTimers(shard_queue_[0], now, &new_min_deadline)) {
      ret = kTimersFired;
    }
    shard_queue_[0]->min_deadline = new_min_deadline;
    OnDeadlineChanged(shard_queue_[0]);
  }
  *next = std::min(*next, shard_queue_[0]->min_deadline);

  in_checking_ = false;
  return ret;
}

void TimerManager::SwapAdjacentShardsInQueue(uint32_t first) {
  TimerShard* tmp = shard_queue_[first];
  shard_queue_[first] = shard_queue_[first + 1];
  shard_queue_[first + 1] = tmp;
  shard_queue_[first]->shard_queue_index = first;
  shard_queue_[first + 1]->shard_queue_index = first + 1;
}

void TimerManager::OnDeadlineChanged(TimerShard* shard) {
  while (shard->shard_queue_index > 0 &&
         shard->min_deadline <
             shard_queue_[shard->shard_queue_index - 1]->min_deadline) {
    SwapAdjacentShardsInQueue(shard->shard_queue_index - 1);
  }
  while (shard->shard_queue_index < NUM_SHARDS - 1 &&
         shard->min_deadline >
             shard_queue_[shard->shard_queue_index + 1]->min_deadline) {
    SwapAdjacentShardsInQueue(shard->shard_queue_index);
  }
}

static double clamp(double val, double min, double max) {
  if (val < min) return min;
  if (val > max) return max;
  return val;
}

bool TimerManager::RefillHeap(TimerShard* shard, Time now) {
  // Compute the new queue window width and bound by the limits
  double computed_deadline_delta =
      shard->stats.UpdateAverage() * ADD_DEADLINE_SCALE;
  double deadline_delta =
      clamp(computed_deadline_delta, MIN_QUEUE_WINDOW_DURATION,
            MAX_QUEUE_WINDOW_DURATION);

  // Compute the new cap and pull all timers under it into the queue
  shard->queue_deadline_cap =
      std::max(now, shard->min_deadline) +
      Time::Delta::FromMilliseconds(
          static_cast<int64_t>(deadline_delta * 1000.0));

  Timer *timer, *next;
  for (timer = LIST_FIRST(&shard->less_urgent_timers); timer; timer = next) {
    next = LIST_NEXT(timer, entry_);
    if (timer->deadline() < shard->queue_deadline_cap) {
      LIST_REMOVE(timer, entry_);
      timer->entry_.le_prev = nullptr;
      shard->urgent_timers.Add(timer);
    }
  }
  return !shard->urgent_timers.empty();
}

Timer* TimerManager::PopOne(TimerShard* shard, Time now) {
  for (;;) {
    if (shard->urgent_timers.empty()) {
      if (now < shard->queue_deadline_cap) {
        return nullptr;
      }
      if (!RefillHeap(shard, now)) {
        return nullptr;
      }
    }
    Timer* timer = shard->urgent_timers.Top();
    if (timer->deadline_ > now) {
      return nullptr;
    }
    timer->pending_ = false;
    shard->urgent_timers.Pop();
    return timer;
  }
}

size_t TimerManager::PopTimers(TimerShard* shard, Time now,
                               Time* new_min_deadline) {
  size_t n = 0;
  Timer* timer;
  MutexLock lock(&shard->mutex);
  while ((timer = PopOne(shard, now))) {
    timer->controller_->scheduled_.reset(
        new TaskHandle(TaskRunner::Get()->PostTask(timer->closure_)));
    ++n;
  }
  *new_min_deadline = shard->ComputeMinDeadline();
  return n;
}

TimerManager::TimeAveragedStats::TimeAveragedStats(double init_avg,
                                                   double regress_weight,
                                                   double persistence_factor)
    : init_avg_(init_avg),
      regress_weight_(regress_weight),
      persistence_factor_(persistence_factor),
      batch_total_value_(0.0),
      batch_num_samples_(0.0),
      aggregate_total_weight_(0.0),
      aggregate_weighted_avg_(init_avg) {}

void TimerManager::TimeAveragedStats::AddSample(double value) {
  batch_total_value_ += value;
  ++batch_num_samples_;
}

double TimerManager::TimeAveragedStats::UpdateAverage() {
  // Start with the current batch
  double weighted_sum = batch_total_value_;
  double total_weight = batch_num_samples_;
  if (regress_weight_ > 0) {
    // Add in the regression towards init_avg_
    weighted_sum += regress_weight_ * init_avg_;
    total_weight += regress_weight_;
  }
  if (persistence_factor_ > 0) {
    // Add in the persistence
    const double prev_sample_weight =
        persistence_factor_ * aggregate_total_weight_;
    weighted_sum += prev_sample_weight * aggregate_weighted_avg_;
    total_weight += prev_sample_weight;
  }
  aggregate_weighted_avg_ =
      (total_weight > 0) ? (weighted_sum / total_weight) : init_avg_;
  aggregate_total_weight_ = total_weight;
  batch_num_samples_ = 0;
  batch_total_value_ = 0;
  return aggregate_weighted_avg_;
}

TimerManager::TimerShard::TimerShard()
    : mutex(),
      stats(1.0 / ADD_DEADLINE_SCALE, 0.1, 0.5),
      queue_deadline_cap(Time::Zero()),
      min_deadline(Time::Zero()),
      shard_queue_index(-1),
      urgent_timers() {
  LIST_INIT(&less_urgent_timers);
}

Time TimerManager::TimerShard::ComputeMinDeadline() {
  return urgent_timers.empty()
             ? (queue_deadline_cap + Time::Delta::FromSeconds(1))
             : urgent_timers.Top()->deadline();
}

}  // namespace iomgr
