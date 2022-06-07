#include "timer/timer_heap.h"

#include <glog/logging.h>
#include <gtest/gtest.h>

#include "iomgr/timer.h"

namespace iomgr {

const int kNumTimers = 20;
static const Time now = Time::Now();

class TimerHeapTest : public testing::Test {
 protected:
  struct TimerWrapper {
    TimerWrapper() : timer(), inserted(false) {}
    TimerWrapper(Time deadline) : timer(), inserted(false) {
      timer.deadline_ = deadline;
    }

    Timer timer;
    bool inserted;
  };

  static Time ReadomeDeadline() {
    return now + Time::Delta::FromMilliseconds(rand());
  }
  void CreateTimers(const size_t count) {
    DCHECK_GT(count, 0);
    timers_ = std::vector<TimerWrapper>(count);

    for (int i = 0; i < count; ++i) {
      new (&timers_[i]) TimerWrapper(ReadomeDeadline());
    }
  }

  TimerWrapper* SearchOne(bool inserted) {
    const size_t count = timers_.size();
    std::vector<size_t> search_order(count);
    for (size_t i = 0; i < count; ++i) {
      search_order[i] = i;
    }
    for (size_t i = 0; i < count * 2; ++i) {
      size_t a = static_cast<size_t>(rand()) % count;
      size_t b = static_cast<size_t>(rand()) % count;
      std::swap(search_order[a], search_order[b]);
    }

    TimerWrapper* ret = nullptr;
    for (size_t i = 0; ret == nullptr && i < count; ++i) {
      if (timers_[search_order[i]].inserted == inserted) {
        ret = &timers_[search_order[i]];
      }
    }
    return ret;
  }

  std::vector<TimerWrapper> timers_;
};

static Time RandomDeadline() {
  return now + Time::Delta::FromMilliseconds(rand());
}

TEST_F(TimerHeapTest, Add) {
  TimerHeap heap;
  CreateTimers(kNumTimers);
  for (int i = 0; i < kNumTimers; ++i) {
    EXPECT_FALSE(heap.Contains(&timers_[i].timer));
    heap.Add(&timers_[i].timer);
    EXPECT_EQ(i + 1, heap.timer_count());
    EXPECT_TRUE(heap.Contains(&timers_[i].timer));
    EXPECT_TRUE(heap.CheckValid());
  }
}

TEST_F(TimerHeapTest, Top) {
  TimerHeap heap;
  CreateTimers(kNumTimers);
  TimerWrapper* min = &timers_[0];
  for (size_t i = 0; i < kNumTimers; ++i) {
    heap.Add(&timers_[i].timer);
    if (timers_[i].timer.deadline() < min->timer.deadline()) {
      min = &timers_[i];
    }
    EXPECT_EQ(&min->timer, heap.Top());
  }
}

TEST_F(TimerHeapTest, Remove) {
  TimerHeap heap;
  CreateTimers(kNumTimers);
  for (size_t i = 0; i < kNumTimers; ++i) {
    heap.Add(&timers_[i].timer);
  }

  int remainder = kNumTimers;
  for (size_t i = 0; i < kNumTimers; ++i) {
    heap.Remove(&timers_[i].timer);
    EXPECT_FALSE(heap.Contains(&timers_[i].timer));
    CHECK_EQ(--remainder, heap.timer_count());
    EXPECT_TRUE(heap.CheckValid());
  }
}

TEST_F(TimerHeapTest, Add_Remove) {
  TimerHeap heap;
  const size_t kOps = 10000;
  const size_t kNumTimers2 = kNumTimers * 100;
  CreateTimers(kNumTimers2);
  size_t num_inserted = 0;

  for (size_t round = 0; round < kOps; round++) {
    int r = rand() % 1000;
    if (r < 550) {
      // 55% of the time we try to add something
      TimerWrapper* timer = SearchOne(false);
      if (timer != nullptr) {
        heap.Add(&timer->timer);
        timer->inserted = true;
        ++num_inserted;
        EXPECT_TRUE(heap.CheckValid());
      }
    } else if (r <= 650) {
      // 10% of the time we try to remove something
      TimerWrapper* timer = SearchOne(true);
      if (timer != nullptr) {
        heap.Remove(&timer->timer);
        timer->inserted = false;
        --num_inserted;
        EXPECT_TRUE(heap.CheckValid());
      }
    } else {
      // the remaining time we pop
      if (num_inserted > 0) {
        Timer* top = heap.Top();
        heap.Pop();
        for (size_t i = 0; i < kNumTimers2; ++i) {
          if (top == &timers_[i].timer) {
            EXPECT_TRUE(timers_[i].inserted);
            timers_[i].inserted = false;
          }
        }
        --num_inserted;
        EXPECT_TRUE(heap.CheckValid());
      }
    }

    if (num_inserted) {
      TimerWrapper* min = nullptr;
      for (size_t i = 0; i < kNumTimers2; ++i) {
        if (timers_[i].inserted) {
          if (min == nullptr) {
            min = &timers_[i];
          } else if (timers_[i].timer.deadline() < min->timer.deadline()) {
            min = &timers_[i];
          }
        }
      }
      EXPECT_EQ(heap.Top(), &min->timer);
    }
  }
}

}  // namespace iomgr

int main(int argc, char** argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
