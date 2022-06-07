#ifndef LIBIOMGR_UTIL_AVERAGED_STATS_H_
#define LIBIOMGR_UTIL_AVERAGED_STATS_H_

namespace iomgr {

class AveragedStats {
 public:
  AveragedStats(double init_avg, double regress_weight,
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

}  // namespace iomgr

#endif  // LIBIOMGR_UTIL_AVERAGED_STATS_H_