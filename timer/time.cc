// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "iomgr/time.h"

#include <sys/time.h>

namespace iomgr {

std::string Time::Delta::ToDebuggingValue() const {
  constexpr int64_t kMillisecondInMicroseconds = 1000;
  constexpr int64_t kSecondInMicroseconds = 1000 * kMillisecondInMicroseconds;

  int64_t absolute_value = std::abs(time_offset_);

  // For debugging purposes, always display the value with the highest precision
  // available.
  if (absolute_value >= kSecondInMicroseconds &&
      absolute_value % kSecondInMicroseconds == 0) {
    return std::to_string(time_offset_ / kSecondInMicroseconds) + "s";
  }
  if (absolute_value >= kMillisecondInMicroseconds &&
      absolute_value % kMillisecondInMicroseconds == 0) {
    return std::to_string(time_offset_ / kMillisecondInMicroseconds) + "ms";
  }
  return std::to_string(time_offset_) + "us";
}

Time Time::Now() {
  int64_t usec = 0;
  struct timespec now;
  if (clock_gettime(CLOCK_MONOTONIC, &now) == 0) {
    usec = now.tv_sec * 1000000 + now.tv_nsec / 1000;
  }
  return Time(usec);
}

WallTime WallTime::Now() {
  uint64_t usec = 0;
  struct timeval now;
  if (gettimeofday(&now, NULL) == 0) {
    usec = now.tv_sec * 1000000 + now.tv_usec;
  }
  return WallTime(usec);
}

uint64_t WallTime::ToUNIXSeconds() const { return microseconds_ / 1000000; }

uint64_t WallTime::ToUNIXMicroseconds() const { return microseconds_; }

bool WallTime::IsAfter(WallTime other) const {
  return microseconds_ > other.microseconds_;
}

bool WallTime::IsBefore(WallTime other) const {
  return microseconds_ < other.microseconds_;
}

bool WallTime::IsZero() const { return microseconds_ == 0; }

Time::Delta WallTime::AbsoluteDifference(WallTime other) const {
  uint64_t d;

  if (microseconds_ > other.microseconds_) {
    d = microseconds_ - other.microseconds_;
  } else {
    d = other.microseconds_ - microseconds_;
  }

  if (d > static_cast<uint64_t>(std::numeric_limits<int64_t>::max())) {
    d = std::numeric_limits<int64_t>::max();
  }
  return Time::Delta::FromMicroseconds(d);
}

WallTime WallTime::Add(Time::Delta delta) const {
  uint64_t microseconds = microseconds_ + delta.ToMicroseconds();
  if (microseconds < microseconds_) {
    microseconds = std::numeric_limits<uint64_t>::max();
  }
  return WallTime(microseconds);
}

// TODO(ianswett) Test this.
WallTime WallTime::Subtract(Time::Delta delta) const {
  uint64_t microseconds = microseconds_ - delta.ToMicroseconds();
  if (microseconds > microseconds_) {
    microseconds = 0;
  }
  return WallTime(microseconds);
}

}  // namespace iomgr
