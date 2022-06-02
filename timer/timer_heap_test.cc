#include "timer/timer_heap.h"

#include "glog/logging.h"
#include "gtest/gtest.h"
#include "iomgr/time.h"
#include "iomgr/timer.h"
#include "timer/test/timer_test.h"

namespace iomgr {

const int kNumTimers = 20;
static const Time now = Time::Now();

static Time RandomDeadline() {
  return now + Time::Delta::FromMilliseconds(rand());
}

static TimerTest* CreateTestElements(int count) {
  DCHECK_GE(count, 0);
  TimerTest* timers = new TimerTest[count];
  for (int i = 0; i < count; ++i) {
    new (&timers[i]) TimerTest(RandomDeadline());
  }
  return timers;
}

TEST(TimerHeapTest, Add) {
  TimerHeap heap;
  TimerTest* timers = CreateTestElements(kNumTimers);
  for (int i = 0; i < kNumTimers; ++i) {
    EXPECT_FALSE(heap.Contains(timers[i].timer()));
    heap.Add(timers[i].timer());
    EXPECT_EQ(i + 1, heap.timer_count());
    EXPECT_TRUE(heap.Contains(timers[i].timer()));
    EXPECT_TRUE(heap.CheckValid());
  }
  delete[] timers;
}

TEST(TimerHeapTest, Top) {
  TimerHeap heap;
  TimerTest* timers = CreateTestElements(kNumTimers);
  TimerTest* min = &timers[0];
  for (size_t i = 0; i < kNumTimers; ++i) {
    heap.Add(timers[i].timer());
    if (timers[i].deadline() < min->deadline()) {
      min = &timers[i];
    }
    EXPECT_EQ(min->timer(), heap.Top());
  }
  delete[] timers;
}

TEST(TimerHeapTest, Remove) {
  TimerHeap heap;
  TimerTest* timers = CreateTestElements(kNumTimers);
  for (size_t i = 0; i < kNumTimers; ++i) {
    heap.Add(timers[i].timer());
  }

  int remainder = kNumTimers;
  for (size_t i = 0; i < kNumTimers; ++i) {
    heap.Remove(timers[i].timer());
    EXPECT_FALSE(heap.Contains(timers[i].timer()));
    CHECK_EQ(--remainder, heap.timer_count());
    EXPECT_TRUE(heap.CheckValid());
  }
  delete[] timers;
}

static TimerTest* SearchElems(TimerTest* timers, int count, bool inserted) {
  std::vector<size_t> search_order(count);
  for (size_t i = 0; i < count; ++i) {
    search_order[i] = i;
  }
  for (size_t i = 0; i < count * 2; ++i) {
    size_t a = static_cast<size_t>(rand()) % count;
    size_t b = static_cast<size_t>(rand()) % count;
    std::swap(search_order[a], search_order[b]);
  }

  TimerTest* out = nullptr;
  for (size_t i = 0; out == nullptr && i < count; ++i) {
    if (timers[search_order[i]].inserted() == inserted) {
      out = &timers[search_order[i]];
    }
  }
  return out;
}

TEST(TimerHeapTest, Add_Remove) {
  TimerHeap heap;
  const size_t kElemsSize = 1000;
  const size_t kOps = 10000;
  TimerTest* timers = CreateTestElements(kElemsSize);
  size_t num_inserted = 0;

  for (size_t round = 0; round < kOps; round++) {
    int r = rand() % 1000;
    if (r < 550) {
      // 55% of the time we try to add something
      TimerTest* timer = SearchElems(timers, kElemsSize, false);
      if (timer != nullptr) {
        heap.Add(timer->timer());
        timer->set_inserted(true);
        ++num_inserted;
        EXPECT_TRUE(heap.CheckValid());
      }
    } else if (r <= 650) {
      // 10% of the time we try to remove something
      TimerTest* timer = SearchElems(timers, kElemsSize, true);
      if (timer != nullptr) {
        heap.Remove(timer->timer());
        timer->set_inserted(false);
        --num_inserted;
        EXPECT_TRUE(heap.CheckValid());
      }
    } else {
      // the remaining time we pop
      if (num_inserted > 0) {
        Timer* top = heap.Top();
        heap.Pop();
        for (size_t i = 0; i < kElemsSize; ++i) {
          if (top == timers[i].timer()) {
            EXPECT_TRUE(timers[i].inserted());
            timers[i].set_inserted(false);
          }
        }
        --num_inserted;
        EXPECT_TRUE(heap.CheckValid());
      }
    }

    if (num_inserted) {
      TimerTest* min = nullptr;
      for (size_t i = 0; i < kElemsSize; ++i) {
        if (timers[i].inserted()) {
          if (min == nullptr) {
            min = &timers[i];
          } else if (timers[i].deadline() < min->deadline()) {
            min = &timers[i];
          }
        }
      }
      EXPECT_EQ(heap.Top(), min->timer());
    }
  }
  delete[] timers;
}

}  // namespace iomgr

int main(int argc, char** argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
