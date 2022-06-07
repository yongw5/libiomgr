#include "util/averaged_stats.h"

namespace iomgr {

AveragedStats::AveragedStats(double init_avg, double regress_weight,
                             double persistence_factor)
    : init_avg_(init_avg),
      regress_weight_(regress_weight),
      persistence_factor_(persistence_factor),
      batch_total_value_(0.0),
      batch_num_samples_(0.0),
      aggregate_total_weight_(0.0),
      aggregate_weighted_avg_(init_avg) {}

void AveragedStats::AddSample(double value) {
  batch_total_value_ += value;
  ++batch_num_samples_;
}

double AveragedStats::UpdateAverage() {
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

}  // namespace iomgr
