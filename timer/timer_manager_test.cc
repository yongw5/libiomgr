#include "timer/timer_manager.h"

#include <math.h>

#include "glog/logging.h"
#include "gtest/gtest.h"
#include "iomgr/time.h"
#include "iomgr/timer.h"
#include "timer/test/timer_test.h"

namespace iomgr {

class TimerManagerTest : public testing::Test {
 public:
  TimerManagerTest() {
    stats = reinterpret_cast<TimerManager::TimeAveragedStats*>(
        new char[sizeof(TimerManager::TimeAveragedStats)]);
  }
  ~TimerManagerTest() { delete[] reinterpret_cast<char*>(stats); }
  void InitStats(double init_avg, double regress_weight,
                 double persistence_factor) {
    new (stats) TimerManager::TimeAveragedStats(init_avg, regress_weight,
                                                persistence_factor);
  }

 protected:
  TimerManager::TimeAveragedStats* stats;
};

TEST_F(TimerManagerTest, TimeAveragedStatsAddSample) {
  InitStats(1000.0, 0.0, 0.0);
  EXPECT_DOUBLE_EQ(1000.0, stats->init_avg());
  EXPECT_DOUBLE_EQ(1000.0, stats->aggregate_weighted_avg());
  EXPECT_DOUBLE_EQ(0.0, stats->aggregate_total_weight());

  stats->AddSample(1000.0);
  EXPECT_DOUBLE_EQ(1000.0, stats->batch_total_value());
  EXPECT_DOUBLE_EQ(1.0, stats->batch_num_samples());

  stats->AddSample(3000.0);
  EXPECT_DOUBLE_EQ(4000.0, stats->batch_total_value());
  EXPECT_DOUBLE_EQ(2.0, stats->batch_num_samples());

  stats->UpdateAverage();
  EXPECT_DOUBLE_EQ(0.0, stats->batch_total_value());
  EXPECT_DOUBLE_EQ(0.0, stats->batch_num_samples());

  stats->AddSample(1000.0);
  EXPECT_DOUBLE_EQ(1000.0, stats->batch_total_value());
  EXPECT_DOUBLE_EQ(1.0, stats->batch_num_samples());
}

TEST_F(TimerManagerTest, TimeAveragedStatsNoRegressNoPersist) {
  // -----------test 1
  InitStats(1000.0, 0.0, 0.0);
  EXPECT_DOUBLE_EQ(1000.0, stats->aggregate_weighted_avg());
  EXPECT_DOUBLE_EQ(0.0, stats->aggregate_total_weight());

  // Should have no effect
  stats->UpdateAverage();
  EXPECT_DOUBLE_EQ(1000.0, stats->aggregate_weighted_avg());
  EXPECT_DOUBLE_EQ(0.0, stats->aggregate_total_weight());

  // Should replace old average
  stats->AddSample(2000.0);
  stats->UpdateAverage();
  EXPECT_DOUBLE_EQ(2000.0, stats->aggregate_weighted_avg());
  EXPECT_DOUBLE_EQ(1.0, stats->aggregate_total_weight());

  stats->AddSample(3000.0);
  stats->UpdateAverage();
  EXPECT_DOUBLE_EQ(3000.0, stats->aggregate_weighted_avg());
  EXPECT_DOUBLE_EQ(1.0, stats->aggregate_total_weight());

  // -----------test 2
  InitStats(1000.0, 0.0, 0.0);

  // Should replace init value
  stats->AddSample(2500.0);
  stats->UpdateAverage();
  EXPECT_DOUBLE_EQ(2500.0, stats->aggregate_weighted_avg());

  stats->AddSample(3500.0);
  stats->AddSample(4500.0);
  stats->UpdateAverage();
  EXPECT_DOUBLE_EQ(4000.0, stats->aggregate_weighted_avg());
  EXPECT_DOUBLE_EQ(2.0, stats->aggregate_total_weight());
}

TEST_F(TimerManagerTest, TimeAveragedStatsSomeRegressNoPersist) {
  // -----------test 1
  InitStats(1000.0, 1.0, 0.0);
  EXPECT_DOUBLE_EQ(1000.0, stats->aggregate_weighted_avg());
  EXPECT_DOUBLE_EQ(0.0, stats->aggregate_total_weight());

  stats->AddSample(2000.0);
  stats->UpdateAverage();
  EXPECT_DOUBLE_EQ(1500.0, stats->aggregate_weighted_avg());
  EXPECT_DOUBLE_EQ(2.0, stats->aggregate_total_weight());

  stats->AddSample(2000.0);
  stats->UpdateAverage();
  EXPECT_DOUBLE_EQ(1500.0, stats->aggregate_weighted_avg());
  EXPECT_DOUBLE_EQ(2.0, stats->aggregate_total_weight());

  // -----------test 2
  InitStats(1000.0, 0.5, 0.0);
  stats->AddSample(2000.0);
  stats->AddSample(2000.0);
  stats->UpdateAverage();
  // (2 * 2000 + 0.5 * 1000) / 2.5
  EXPECT_DOUBLE_EQ(1800.0, stats->aggregate_weighted_avg());
  EXPECT_DOUBLE_EQ(2.5, stats->aggregate_total_weight());
}

TEST_F(TimerManagerTest, TimeAveragedStatsNoRegressFullPersist) {
  InitStats(1000.0, 0.0, 1.0);
  EXPECT_DOUBLE_EQ(1000.0, stats->aggregate_weighted_avg());
  EXPECT_DOUBLE_EQ(0.0, stats->aggregate_total_weight());

  // Should replace old average
  stats->AddSample(2000.0);
  stats->UpdateAverage();
  EXPECT_DOUBLE_EQ(2000.0, stats->aggregate_weighted_avg());
  EXPECT_DOUBLE_EQ(1.0, stats->aggregate_total_weight());

  // Will result in average of the 3 samples
  stats->AddSample(2300.0);
  stats->AddSample(2300.0);
  stats->UpdateAverage();
  EXPECT_DOUBLE_EQ(2200.0, stats->aggregate_weighted_avg());
  EXPECT_DOUBLE_EQ(3.0, stats->aggregate_total_weight());
}

TEST_F(TimerManagerTest, TimeAveragedStatsNoRegressSomePersist) {
  InitStats(1000.0, 0.0, 0.5);

  // Should replace old average
  stats->AddSample(2000.0);
  stats->UpdateAverage();
  EXPECT_DOUBLE_EQ(2000.0, stats->aggregate_weighted_avg());
  EXPECT_DOUBLE_EQ(1.0, stats->aggregate_total_weight());

  // Will result in average of the 3 samples
  stats->AddSample(2500.0);
  stats->AddSample(4000.0);
  stats->UpdateAverage();
  EXPECT_DOUBLE_EQ(3000.0, stats->aggregate_weighted_avg());
  EXPECT_DOUBLE_EQ(2.5, stats->aggregate_total_weight());
}

TEST_F(TimerManagerTest, TimeAverageStatsSomeRegressSomePersist) {
  InitStats(1000.0, 0.4, 0.6);
  // Sample weight = 0
  EXPECT_DOUBLE_EQ(1000.0, stats->aggregate_weighted_avg());
  EXPECT_DOUBLE_EQ(0.0, stats->aggregate_total_weight());

  stats->UpdateAverage();
  // (0.6 * 0 * 1000 + 0.4 * 1000 / 0.4)
  EXPECT_DOUBLE_EQ(1000.0, stats->aggregate_weighted_avg());
  EXPECT_DOUBLE_EQ(0.4, stats->aggregate_total_weight());

  stats->AddSample(2640.0);
  stats->UpdateAverage();
  // 1 * 2640 + 0.6 * 0.4 * 1000 + 0.4 * 1000 / (1 + 0.6 * 0.4 + 0.4)
  EXPECT_DOUBLE_EQ(2000.0, stats->aggregate_weighted_avg());
  EXPECT_DOUBLE_EQ(1.64, stats->aggregate_total_weight());

  stats->AddSample(2876.8);
  stats->UpdateAverage();
  // (1 * 2876.8 + 0.6 * 1.64 * 2000 + 0.4 * 1000 / (1 + 0.6 * 1.64 + 0.4)
  EXPECT_DOUBLE_EQ(2200.0, stats->aggregate_weighted_avg());
  EXPECT_DOUBLE_EQ(2.384, stats->aggregate_total_weight());

  stats->AddSample(4944.32);
  stats->UpdateAverage();
  /* (1 * 4944.32 + 0.6 * 2.384 * 2200 + 0.4 * 1000) /
     (1 + 0.6 * 2.384 + 0.4) */
  EXPECT_DOUBLE_EQ(3000, stats->aggregate_weighted_avg());
  EXPECT_DOUBLE_EQ(2.8304, stats->aggregate_total_weight());
}

static Time::Delta RandomDelta(Time::Delta base) {
  return base + Time::Delta::FromMilliseconds(rand() % 1000);
}

// TODO(WUYONG) TimerManagerTest
const int kNumElements = 20;
static Time now = Time::Now();
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

static TimerTest* SearchElems(TimerTest* timers, size_t count, bool inserted) {
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

}  // namespace iomgr

int main(int argc, char** argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
