// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBIOMGR_INCLUDE_TIME_H_
#define LIBIOMGR_INCLUDE_TIME_H_

#include <math.h>
#include <stdint.h>

#include <limits>
#include <ostream>
#include <string>

#include "iomgr/export.h"

namespace iomgr {

// Time represents one point in time, stored in microsecond resolution, and is
// monotonically increasing, even across system clock adjustments.
//
// A Time is a purely relative time and cannot be compared to each other unless
// two Times are from the same clocks. If you need an absolute time, see
// WallTime, below.
class IOMGR_EXPORT Time {
 public:
  // A Time::Delta represents the signed difference between two points in
  // time, stored in microsecond resolution.
  class IOMGR_EXPORT Delta {
   public:
    // Create a object with an offset of 0.
    static Delta Zero() { return Delta(0); }

    // Create a object wth infinite offset time
    static Delta Inifinite() { return Delta(kInfiniteTimeUs); }

    // Converts a number of seconds to a time offset.
    static Delta FromSeconds(int64_t secs) { return Delta(secs * 1000 * 1000); }

    // Converts a number of milliseconds to a time offset.
    static Delta FromMilliseconds(int64_t ms) { return Delta(ms * 1000); }

    // Converts a number of microseconds to a time offset.
    static Delta FromMicroseconds(int64_t us) { return Delta(us); }

    // Converts the time offset to a rounded number of seconds.
    inline int64_t ToSeconds() const { return time_offset_ / 1000 / 1000; }

    // Converts the time offset to a rounded number of milliseconds.
    inline int64_t ToMilliseconds() const { return time_offset_ / 1000; }

    // Converts the time offset to a rounded number of microseconds.
    inline int64_t ToMicroseconds() const { return time_offset_; }

    inline bool IsZero() const { return time_offset_ == 0; }

    inline bool IsInfinite() const { return time_offset_ == kInfiniteTimeUs; }

    std::string ToDebuggingValue() const;

   private:
    friend inline bool operator==(Time::Delta lhs, Time::Delta rhs);
    friend inline bool operator<(Time::Delta lhs, Time::Delta rhs);
    friend inline Time::Delta operator<<(Time::Delta lhs, size_t rhs);
    friend inline Time::Delta operator>>(Time::Delta lhs, size_t rhs);

    friend inline Time::Delta operator+(Time::Delta lhs, Time::Delta rhs);
    friend inline Time::Delta operator-(Time::Delta lhs, Time::Delta rhs);
    friend inline Time::Delta operator*(Time::Delta lhs, int rhs);
    friend inline Time::Delta operator*(Time::Delta lhs, double rhs);

    friend inline Time operator+(Time lhs, Time::Delta rhs);
    friend inline Time operator-(Time lhs, Time::Delta rhs);
    friend inline Time::Delta operator-(Time lhs, Time rhs);

    static const int64_t kInfiniteTimeUs = std::numeric_limits<int64_t>::max();

    explicit Delta(int64_t time_offset) : time_offset_(time_offset) {}

    int64_t time_offset_;
    friend class Time;
  };

  // Creates a new Time represents now point, got from gettime with
  // CLOCK_MONOTONIC flag
  static Time Now();

  // Creates a new Time with an internal value of 0.  IsInitialized()
  // will return false for these times.
  static Time Zero() { return Time(0); }

  // Creates a new Time with an infinite time
  static Time Infinite() { return Time(Delta::kInfiniteTimeUs); }

  Time(const Time& other) = default;

  Time& operator=(const Time& other) {
    time_ = other.time_;
    return *this;
  }

  bool IsZero() const { return time_ == 0; }

  bool IsInfinite() const { return time_ == Delta::kInfiniteTimeUs; }

  // Produce the internal value to be used when logging.  This value
  // represents the number of microseconds since some epoch.
  inline int64_t ToDebuggingValue() const { return time_; }

 private:
  friend inline bool operator==(Time lhs, Time rhs);
  friend inline bool operator<(Time lhs, Time rhs);
  friend inline Time operator+(Time lhs, Time::Delta rhs);
  friend inline Time operator-(Time lhs, Time::Delta rhs);
  friend inline Time::Delta operator-(Time lhs, Time rhs);

  explicit Time(int64_t time) : time_(time) {}

  int64_t time_;
};

// A WallTime represents an absolute time that is globally consistent. In
// practice, clock-skew means that comparing values from different machines
// requires some flexibility.
class IOMGR_EXPORT WallTime {
 public:
  // Creates a new WallTime represents now point
  static WallTime Now();

  // FromUNIXSeconds constructs a WallTime from a count of the seconds
  // since the UNIX epoch.
  static WallTime FromUNIXSeconds(uint64_t seconds) {
    return WallTime(seconds * 1000000);
  }

  static WallTime FromUNIXMicroseconds(uint64_t microseconds) {
    return WallTime(microseconds);
  }

  // Zero returns a WallTime set to zero. IsZero will return true for this
  // value.
  static WallTime Zero() { return WallTime(0); }

  // Returns the number of seconds since the UNIX epoch.
  uint64_t ToUNIXSeconds() const;
  // Returns the number of microseconds since the UNIX epoch.
  uint64_t ToUNIXMicroseconds() const;

  bool IsAfter(WallTime other) const;
  bool IsBefore(WallTime other) const;

  // IsZero returns true if this object is the result of calling |Zero|.
  bool IsZero() const;

  // AbsoluteDifference returns the absolute value of the time difference
  // between |this| and |other|.
  Time::Delta AbsoluteDifference(WallTime other) const;

  // Add returns a new WallTime that represents the time of |this| plus
  // |delta|.
  WallTime Add(Time::Delta delta) const;

  // Subtract returns a new WallTime that represents the time of |this|
  // minus |delta|.
  WallTime Subtract(Time::Delta delta) const;

  bool operator==(const WallTime& other) const {
    return microseconds_ == other.microseconds_;
  }

  Time::Delta operator-(const WallTime& rhs) const {
    return Time::Delta::FromMicroseconds(microseconds_ - rhs.microseconds_);
  }

 private:
  explicit WallTime(uint64_t microseconds) : microseconds_(microseconds) {}

  uint64_t microseconds_;
};

// Non-member relational operators for Time::Delta.
inline bool operator==(Time::Delta lhs, Time::Delta rhs) {
  return lhs.time_offset_ == rhs.time_offset_;
}
inline bool operator!=(Time::Delta lhs, Time::Delta rhs) {
  return !(lhs == rhs);
}
inline bool operator<(Time::Delta lhs, Time::Delta rhs) {
  return lhs.time_offset_ < rhs.time_offset_;
}
inline bool operator>(Time::Delta lhs, Time::Delta rhs) { return rhs < lhs; }
inline bool operator<=(Time::Delta lhs, Time::Delta rhs) {
  return !(rhs < lhs);
}
inline bool operator>=(Time::Delta lhs, Time::Delta rhs) {
  return !(lhs < rhs);
}
inline Time::Delta operator<<(Time::Delta lhs, size_t rhs) {
  return Time::Delta(lhs.time_offset_ << rhs);
}
inline Time::Delta operator>>(Time::Delta lhs, size_t rhs) {
  return Time::Delta(lhs.time_offset_ >> rhs);
}

// Non-member relational operators for Time.
inline bool operator==(Time lhs, Time rhs) { return lhs.time_ == rhs.time_; }
inline bool operator!=(Time lhs, Time rhs) { return !(lhs == rhs); }
inline bool operator<(Time lhs, Time rhs) { return lhs.time_ < rhs.time_; }
inline bool operator>(Time lhs, Time rhs) { return rhs < lhs; }
inline bool operator<=(Time lhs, Time rhs) { return !(rhs < lhs); }
inline bool operator>=(Time lhs, Time rhs) { return !(lhs < rhs); }

// Override stream output operator for gtest or QUICHE_CHECK macros.
inline std::ostream& operator<<(std::ostream& output, const Time t) {
  output << t.ToDebuggingValue();
  return output;
}

// Non-member arithmetic operators for Time::Delta.
inline Time::Delta operator+(Time::Delta lhs, Time::Delta rhs) {
  return Time::Delta(lhs.time_offset_ + rhs.time_offset_);
}
inline Time::Delta operator-(Time::Delta lhs, Time::Delta rhs) {
  return Time::Delta(lhs.time_offset_ - rhs.time_offset_);
}
inline Time::Delta operator*(Time::Delta lhs, int rhs) {
  return Time::Delta(lhs.time_offset_ * rhs);
}
inline Time::Delta operator*(Time::Delta lhs, double rhs) {
  return Time::Delta(static_cast<int64_t>(
      std::llround(static_cast<double>(lhs.time_offset_) * rhs)));
}
inline Time::Delta operator*(int lhs, Time::Delta rhs) { return rhs * lhs; }
inline Time::Delta operator*(double lhs, Time::Delta rhs) { return rhs * lhs; }

// Non-member arithmetic operators for Time and Time::Delta.
inline Time operator+(Time lhs, Time::Delta rhs) {
  return Time(lhs.time_ + rhs.time_offset_);
}
inline Time operator-(Time lhs, Time::Delta rhs) {
  return Time(lhs.time_ - rhs.time_offset_);
}
inline Time::Delta operator-(Time lhs, Time rhs) {
  return Time::Delta(lhs.time_ - rhs.time_);
}

// Override stream output operator for gtest.
inline std::ostream& operator<<(std::ostream& output, const Time::Delta delta) {
  output << delta.ToDebuggingValue();
  return output;
}
}  // namespace iomgr

#endif  // LIBIOMGR_INCLUDE_TIME_H_