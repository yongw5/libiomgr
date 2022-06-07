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
#define SHARD_MASK (NUM_SHARDS - 1)

namespace iomgr {

const uint32_t kInvalidIndex = 0xffffffffu;

TimerManager* TimerManager::Get() {
  static TimerManager s_timer_manager;
  IOManager::Get();

  return &s_timer_manager;
}

TimerManager::TimerManager()
    : mutex_(),
      shards_(NUM_SHARDS),
      shard_queue_(NUM_SHARDS) {
  for (int i = 0; i < NUM_SHARDS; ++i) {
    TimerShard* shard = &shards_[i];
    shard->heap_capacity = Time::Infinite();
    shard->shard_queue_index = i;
    shard->min_deadline = Time::Infinite();
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
  TimerShard* shard = &shards_[(PointerHash<Timer*>{}(timer)) & SHARD_MASK];
  {
    MutexLock lock(&shard->mutex);
    timer->deadline_ = deadline;
    timer->pending_ = true;
    timer->closure_ = std::move(closure);
    timer->controller_ = controller;
    shard->stats.AddSample(static_cast<double>(timeout.ToSeconds()));
    if (deadline < shard->heap_capacity) {
      is_first_timer = shard->urgent_timers.Add(timer);
    } else {
      timer->heap_index_ = kInvalidIndex;
      LIST_INSERT_HEAD(&shard->less_urgent_timers, timer, entry_);
    }
  }

  MutexLock lock(&mutex_);
  if (is_first_timer && deadline < shard->min_deadline) {
    shard->min_deadline = deadline;
    OnDeadlineChanged(shard);
  }
}

void TimerManager::TimerCancel(Timer::Controller* controller) {
  DCHECK(controller);

  bool is_first_timer = false;
  Timer* timer = controller->timer();
  TimerShard* shard = &shards_[(PointerHash<Timer*>{}(timer)) & SHARD_MASK];

  MutexLock lock(&shard->mutex);
  if (!timer->pending()) {
    return;
  }
  timer->pending_ = false;
  if (timer->heap_index_ == kInvalidIndex) {
    LIST_REMOVE(timer, entry_);
    timer->entry_.le_prev = nullptr;
  } else {
    shard->urgent_timers.Remove(timer);
  }
  controller->scheduled_.reset();
}

Time::Delta TimerManager::TimerCheck() {
  MutexLock lock(&mutex_);
  Time now = Time::Now();
  while (shard_queue_[0]->min_deadline <= now) {
    Time new_min_deadline = Time::Zero();
    PopTimers(shard_queue_[0], now, &new_min_deadline);
    shard_queue_[0]->min_deadline = new_min_deadline;
    OnDeadlineChanged(shard_queue_[0]);
  }
  Time::Delta timeout = Time::Delta::Inifinite();
  Time min_deadline = shard_queue_[0]->min_deadline;
  if (!min_deadline.IsInfinite()) {
    timeout = min_deadline - now;
  }
  return timeout;
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
  IOManager::Get()->Wakeup();
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
  if (shard->heap_capacity.IsInfinite()) {
  }
  shard->heap_capacity = std::max(now, shard->heap_capacity) +
                         Time::Delta::FromMilliseconds(
                             static_cast<int64_t>(deadline_delta * 1000.0));

  Timer *timer, *next;
  for (timer = LIST_FIRST(&shard->less_urgent_timers); timer; timer = next) {
    next = LIST_NEXT(timer, entry_);
    if (timer->deadline() < shard->heap_capacity) {
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
      if (now < shard->heap_capacity) {
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
    timer->controller_->scheduled_.reset();
    timer->controller_->scheduled_.reset(
         new TaskHandle(TaskRunner::Get()->PostTask(timer->closure_)));
    ++n;
  }
  *new_min_deadline = shard->ComputeMinDeadline();
  return n;
}

TimerManager::TimerShard::TimerShard()
    : mutex(),
      stats(1.0 / ADD_DEADLINE_SCALE, 0.1, 0.5),
      heap_capacity(Time::Zero()),
      min_deadline(Time::Zero()),
      shard_queue_index(-1),
      urgent_timers() {
  LIST_INIT(&less_urgent_timers);
}

Time TimerManager::TimerShard::ComputeMinDeadline() {
  return urgent_timers.empty() ? Time::Infinite()
                               : urgent_timers.Top()->deadline();
}

}  // namespace iomgr
