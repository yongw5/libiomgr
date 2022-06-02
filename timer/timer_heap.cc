#include "timer_heap.h"

#include <algorithm>

#include "glog/logging.h"
#include "iomgr/timer.h"

namespace iomgr {

bool TimerHeap::Add(Timer* timer) {
  timer->heap_index_ = timer_count();
  timers_.push_back(timer);
  AdjustUpwards(timer->heap_index_);
  return timer->heap_index_ == 0;
}

void TimerHeap::Remove(Timer* timer) {
  uint32_t i = timer->heap_index_;
  if (i == timer_count() - 1) {
    timers_.pop_back();
  } else {
    timers_[i] = timers_[timer_count() - 1];
    timers_[i]->heap_index_ = i;
    timers_.pop_back();
    uint32_t parent = static_cast<uint32_t>((static_cast<int>(i) - 1) / 2);
    if (timers_[parent]->deadline_ > timers_[i]->deadline_) {
      AdjustUpwards(i);
    } else {
      AdjustDownwards(i);
    }
  }
  MaybeShrink();
}

Timer* TimerHeap::Top() { return timers_[0]; }

void TimerHeap::Pop() { Remove(Top()); }

void TimerHeap::AdjustUpwards(uint32_t i) {
  Timer* t = timers_[i];
  while (i > 0) {
    uint32_t parent = static_cast<uint32_t>((static_cast<int>(i) - 1) / 2);
    if (timers_[parent]->deadline_ <= t->deadline_) {
      break;
    }
    timers_[i] = timers_[parent];
    timers_[i]->heap_index_ = i;
    i = parent;
  }
  timers_[i] = t;
  t->heap_index_ = i;
}

void TimerHeap::AdjustDownwards(uint32_t i) {
  Timer* t = timers_[i];
  for (;;) {
    uint32_t left_child = 1u + 2u * i;
    if (left_child >= timer_count()) {
      break;
    }
    uint32_t right_child = left_child + 1;
    uint32_t next_i =
        right_child < timer_count() &&
                timers_[left_child]->deadline_ > timers_[right_child]->deadline_
            ? right_child
            : left_child;
    if (t->deadline_ <= timers_[next_i]->deadline_) {
      break;
    }
    timers_[i] = timers_[next_i];
    timers_[i]->heap_index_ = i;
    i = next_i;
  }
  timers_[i] = t;
  t->heap_index_ = i;
}

const int kShrinkMinElems = 8;
const int kShinkFullnessFactor = 2;

void TimerHeap::MaybeShrink() {
  if (timer_count() >= kShrinkMinElems &&
      timer_count() <= timers_.capacity() / kShinkFullnessFactor / 2) {
    size_t new_capacity = timer_count() * kShinkFullnessFactor;
    std::vector<Timer*> tmp(new_capacity);
    tmp.resize(timer_count());
    std::copy_n(timers_.begin(), timers_.size(), tmp.begin());
    std::swap(timers_, tmp);
  }
}

void TimerHeap::ResetDeadline(Timer* timer, Time deadline) {
  timer->deadline_ = deadline;
}

bool TimerHeap::Contains(Timer* timer) {
  for (size_t i = 0; i < timer_count(); ++i) {
    if (timers_[i] == timer) {
      return true;
    }
  }
  return false;
}

bool TimerHeap::CheckValid() {
  for (size_t i = 0; i < timer_count(); ++i) {
    size_t left_child = 1u + 2u * i;
    size_t right_child = left_child + 1;
    if (left_child < timer_count() &&
        timers_[i]->deadline_ > timers_[left_child]->deadline_) {
      return false;
    }
    if (right_child < timer_count() &&
        timers_[i]->deadline_ > timers_[right_child]->deadline_) {
      return false;
    }
  }
  return true;
}

}  // namespace iomgr