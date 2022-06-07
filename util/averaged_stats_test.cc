#include "util/averaged_stats.h"

#include "glog/logging.h"
#include "gtest/gtest.h"

namespace iomgr {

TEST(AveragedStatsTest, AddSample) {
  AveragedStats stats(1000.0, 0.0, 0.0);
  EXPECT_DOUBLE_EQ(1000.0, stats.init_avg());
  EXPECT_DOUBLE_EQ(1000.0, stats.aggregate_weighted_avg());
  EXPECT_DOUBLE_EQ(0.0, stats.aggregate_total_weight());

  stats.AddSample(1000.0);
  EXPECT_DOUBLE_EQ(1000.0, stats.batch_total_value());
  EXPECT_DOUBLE_EQ(1.0, stats.batch_num_samples());

  stats.AddSample(3000.0);
  EXPECT_DOUBLE_EQ(4000.0, stats.batch_total_value());
  EXPECT_DOUBLE_EQ(2.0, stats.batch_num_samples());

  stats.UpdateAverage();
  EXPECT_DOUBLE_EQ(0.0, stats.batch_total_value());
  EXPECT_DOUBLE_EQ(0.0, stats.batch_num_samples());

  stats.AddSample(1000.0);
  EXPECT_DOUBLE_EQ(1000.0, stats.batch_total_value());
  EXPECT_DOUBLE_EQ(1.0, stats.batch_num_samples());
}

TEST(AveragedStatsTest, NoRegressNoPersist) {
  {
    // -----------test 1
    AveragedStats stats(1000.0, 0.0, 0.0);
    EXPECT_DOUBLE_EQ(1000.0, stats.aggregate_weighted_avg());
    EXPECT_DOUBLE_EQ(0.0, stats.aggregate_total_weight());

    // Should have no effect
    stats.UpdateAverage();
    EXPECT_DOUBLE_EQ(1000.0, stats.aggregate_weighted_avg());
    EXPECT_DOUBLE_EQ(0.0, stats.aggregate_total_weight());

    // Should replace old average
    stats.AddSample(2000.0);
    stats.UpdateAverage();
    EXPECT_DOUBLE_EQ(2000.0, stats.aggregate_weighted_avg());
    EXPECT_DOUBLE_EQ(1.0, stats.aggregate_total_weight());

    stats.AddSample(3000.0);
    stats.UpdateAverage();
    EXPECT_DOUBLE_EQ(3000.0, stats.aggregate_weighted_avg());
    EXPECT_DOUBLE_EQ(1.0, stats.aggregate_total_weight());
  }

  {
    // -----------test 2
    AveragedStats stats(1000.0, 0.0, 0.0);

    // Should replace init value
    stats.AddSample(2500.0);
    stats.UpdateAverage();
    EXPECT_DOUBLE_EQ(2500.0, stats.aggregate_weighted_avg());

    stats.AddSample(3500.0);
    stats.AddSample(4500.0);
    stats.UpdateAverage();
    EXPECT_DOUBLE_EQ(4000.0, stats.aggregate_weighted_avg());
    EXPECT_DOUBLE_EQ(2.0, stats.aggregate_total_weight());
  }
}

TEST(AveragedStats, SomeRegressNoPersist) {
  {
    // -----------test 1
    AveragedStats stats(1000.0, 1.0, 0.0);
    EXPECT_DOUBLE_EQ(1000.0, stats.aggregate_weighted_avg());
    EXPECT_DOUBLE_EQ(0.0, stats.aggregate_total_weight());

    stats.AddSample(2000.0);
    stats.UpdateAverage();
    EXPECT_DOUBLE_EQ(1500.0, stats.aggregate_weighted_avg());
    EXPECT_DOUBLE_EQ(2.0, stats.aggregate_total_weight());

    stats.AddSample(2000.0);
    stats.UpdateAverage();
    EXPECT_DOUBLE_EQ(1500.0, stats.aggregate_weighted_avg());
    EXPECT_DOUBLE_EQ(2.0, stats.aggregate_total_weight());
  }

  {
    // -----------test 2
    AveragedStats stats(1000.0, 0.5, 0.0);
    stats.AddSample(2000.0);
    stats.AddSample(2000.0);
    stats.UpdateAverage();
    // (2 * 2000 + 0.5 * 1000) / 2.5
    EXPECT_DOUBLE_EQ(1800.0, stats.aggregate_weighted_avg());
    EXPECT_DOUBLE_EQ(2.5, stats.aggregate_total_weight());
  }
}

TEST(AveragedStats, NoRegressFullPersist) {
  AveragedStats stats(1000.0, 0.0, 1.0);
  EXPECT_DOUBLE_EQ(1000.0, stats.aggregate_weighted_avg());
  EXPECT_DOUBLE_EQ(0.0, stats.aggregate_total_weight());

  // Should replace old average
  stats.AddSample(2000.0);
  stats.UpdateAverage();
  EXPECT_DOUBLE_EQ(2000.0, stats.aggregate_weighted_avg());
  EXPECT_DOUBLE_EQ(1.0, stats.aggregate_total_weight());

  // Will result in average of the 3 samples
  stats.AddSample(2300.0);
  stats.AddSample(2300.0);
  stats.UpdateAverage();
  EXPECT_DOUBLE_EQ(2200.0, stats.aggregate_weighted_avg());
  EXPECT_DOUBLE_EQ(3.0, stats.aggregate_total_weight());
}

TEST(AveragedStats, NoRegressSomePersist) {
  AveragedStats stats(1000.0, 0.0, 0.5);

  // Should replace old average
  stats.AddSample(2000.0);
  stats.UpdateAverage();
  EXPECT_DOUBLE_EQ(2000.0, stats.aggregate_weighted_avg());
  EXPECT_DOUBLE_EQ(1.0, stats.aggregate_total_weight());

  // Will result in average of the 3 samples
  stats.AddSample(2500.0);
  stats.AddSample(4000.0);
  stats.UpdateAverage();
  EXPECT_DOUBLE_EQ(3000.0, stats.aggregate_weighted_avg());
  EXPECT_DOUBLE_EQ(2.5, stats.aggregate_total_weight());
}

TEST(AveragedStats, SomeRegressSomePersist) {
  AveragedStats stats(1000.0, 0.4, 0.6);
  // Sample weight = 0
  EXPECT_DOUBLE_EQ(1000.0, stats.aggregate_weighted_avg());
  EXPECT_DOUBLE_EQ(0.0, stats.aggregate_total_weight());

  stats.UpdateAverage();
  // (0.6 * 0 * 1000 + 0.4 * 1000 / 0.4)
  EXPECT_DOUBLE_EQ(1000.0, stats.aggregate_weighted_avg());
  EXPECT_DOUBLE_EQ(0.4, stats.aggregate_total_weight());

  stats.AddSample(2640.0);
  stats.UpdateAverage();
  // 1 * 2640 + 0.6 * 0.4 * 1000 + 0.4 * 1000 / (1 + 0.6 * 0.4 + 0.4)
  EXPECT_DOUBLE_EQ(2000.0, stats.aggregate_weighted_avg());
  EXPECT_DOUBLE_EQ(1.64, stats.aggregate_total_weight());

  stats.AddSample(2876.8);
  stats.UpdateAverage();
  // (1 * 2876.8 + 0.6 * 1.64 * 2000 + 0.4 * 1000 / (1 + 0.6 * 1.64 + 0.4)
  EXPECT_DOUBLE_EQ(2200.0, stats.aggregate_weighted_avg());
  EXPECT_DOUBLE_EQ(2.384, stats.aggregate_total_weight());

  stats.AddSample(4944.32);
  stats.UpdateAverage();
  /* (1 * 4944.32 + 0.6 * 2.384 * 2200 + 0.4 * 1000) /
     (1 + 0.6 * 2.384 + 0.4) */
  EXPECT_DOUBLE_EQ(3000, stats.aggregate_weighted_avg());
  EXPECT_DOUBLE_EQ(2.8304, stats.aggregate_total_weight());
}

}  // namespace iomgr

int main(int argc, char** argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}