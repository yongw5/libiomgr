// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "iomgr/time.h"

#include "glog/logging.h"
#include "gtest/gtest.h"

namespace iomgr {

TEST(TimeDeltaTest, Zero) {
  EXPECT_TRUE(Time::Delta::Zero().IsZero());
  EXPECT_FALSE(Time::Delta::Zero().IsInfinite());
  EXPECT_FALSE(Time::Delta::FromMilliseconds(1).IsZero());
}

TEST(TimeDeltaTest, Infinite) {
  EXPECT_TRUE(Time::Delta::Inifinite().IsInfinite());
  EXPECT_FALSE(Time::Delta::FromMilliseconds(1).IsInfinite());
}

TEST(TimeDeltaTest, FromTo) {
  EXPECT_EQ(Time::Delta::FromMilliseconds(1),
            Time::Delta::FromMicroseconds(1000));
  EXPECT_EQ(Time::Delta::FromSeconds(1), Time::Delta::FromMilliseconds(1000));
  EXPECT_EQ(Time::Delta::FromSeconds(1),
            Time::Delta::FromMicroseconds(1000000));

  EXPECT_EQ(1, Time::Delta::FromMicroseconds(1000).ToMilliseconds());
  EXPECT_EQ(2, Time::Delta::FromMilliseconds(2000).ToSeconds());
  EXPECT_EQ(1000, Time::Delta::FromMilliseconds(1).ToMicroseconds());
  EXPECT_EQ(1, Time::Delta::FromMicroseconds(1000).ToMilliseconds());
  EXPECT_EQ(Time::Delta::FromMilliseconds(2000).ToMicroseconds(),
            Time::Delta::FromSeconds(2).ToMicroseconds());
}

TEST(TimeDeltaTest, Add) {
  EXPECT_EQ(Time::Delta::FromMicroseconds(2000),
            Time::Delta::Zero() + Time::Delta::FromMilliseconds(2));
}

TEST(TimeDeltaTest, Subtract) {
  EXPECT_EQ(
      Time::Delta::FromMicroseconds(1000),
      Time::Delta::FromMilliseconds(2) - Time::Delta::FromMilliseconds(1));
}

TEST(TimeDeltaTest, Multiply) {
  int i = 2;
  EXPECT_EQ(Time::Delta::FromMicroseconds(4000),
            Time::Delta::FromMilliseconds(2) * i);
  EXPECT_EQ(Time::Delta::FromMicroseconds(4000),
            i * Time::Delta::FromMilliseconds(2));
  double d = 2;
  EXPECT_EQ(Time::Delta::FromMicroseconds(4000),
            Time::Delta::FromMilliseconds(2) * d);
  EXPECT_EQ(Time::Delta::FromMicroseconds(4000),
            d * Time::Delta::FromMilliseconds(2));

  // Ensure we are rounding correctly within a single-bit level of precision.
  EXPECT_EQ(Time::Delta::FromMicroseconds(5),
            Time::Delta::FromMicroseconds(9) * 0.5);
  EXPECT_EQ(Time::Delta::FromMicroseconds(2),
            Time::Delta::FromMicroseconds(12) * 0.2);
}

TEST(TimeDeltaTest, Max) {
  EXPECT_EQ(Time::Delta::FromMicroseconds(2000),
            std::max(Time::Delta::FromMicroseconds(1000),
                     Time::Delta::FromMicroseconds(2000)));
}

TEST(TimeDeltaTest, NotEqual) {
  EXPECT_TRUE(Time::Delta::FromSeconds(0) != Time::Delta::FromSeconds(1));
  EXPECT_FALSE(Time::Delta::FromSeconds(0) != Time::Delta::FromSeconds(0));
}

TEST(TimeDeltaTest, DebuggingValue) {
  const Time::Delta one_us = Time::Delta::FromMicroseconds(1);
  const Time::Delta one_ms = Time::Delta::FromMilliseconds(1);
  const Time::Delta one_s = Time::Delta::FromSeconds(1);

  EXPECT_EQ("1s", one_s.ToDebuggingValue());
  EXPECT_EQ("3s", (3 * one_s).ToDebuggingValue());
  EXPECT_EQ("1ms", one_ms.ToDebuggingValue());
  EXPECT_EQ("3ms", (3 * one_ms).ToDebuggingValue());
  EXPECT_EQ("1us", one_us.ToDebuggingValue());
  EXPECT_EQ("3us", (3 * one_us).ToDebuggingValue());

  EXPECT_EQ("3001us", (3 * one_ms + one_us).ToDebuggingValue());
  EXPECT_EQ("3001ms", (3 * one_s + one_ms).ToDebuggingValue());
  EXPECT_EQ("3000001us", (3 * one_s + one_us).ToDebuggingValue());
}

TEST(TimeTest, Zero) {
  EXPECT_TRUE(Time::Zero().IsZero());
  EXPECT_FALSE(Time::Zero().IsInfinite());
  EXPECT_FALSE(Time::Now().IsZero());
}

TEST(TimeTest, Infinite) {
  EXPECT_TRUE(Time::Infinite().IsInfinite());
  EXPECT_FALSE(Time::Now().IsInfinite());
}

TEST(TimeTest, CopyConstruct) {
  Time time_1 = Time::Zero() + Time::Delta::FromMilliseconds(1234);
  EXPECT_NE(time_1, Time(Time::Zero()));
  EXPECT_EQ(time_1, Time(time_1));
}

TEST(TimeTest, CopyAssignment) {
  Time time_1 = Time::Zero() + Time::Delta::FromMilliseconds(1234);
  Time time_2 = Time::Zero();
  EXPECT_NE(time_1, time_2);
  time_2 = time_1;
  EXPECT_EQ(time_1, time_2);
}

TEST(TimeTest, Add) {
  Time time_1 = Time::Zero() + Time::Delta::FromMilliseconds(1);
  Time time_2 = Time::Zero() + Time::Delta::FromMilliseconds(2);

  Time::Delta diff = time_2 - time_1;

  EXPECT_EQ(Time::Delta::FromMilliseconds(1), diff);
  EXPECT_EQ(1000, diff.ToMicroseconds());
  EXPECT_EQ(1, diff.ToMilliseconds());
}

TEST(TimeTest, Subtract) {
  Time time_1 = Time::Zero() + Time::Delta::FromMilliseconds(1);
  Time time_2 = Time::Zero() + Time::Delta::FromMilliseconds(2);

  EXPECT_EQ(Time::Delta::FromMilliseconds(1), time_2 - time_1);
}

TEST(TimeTest, SubtractDelta) {
  Time time = Time::Zero() + Time::Delta::FromMilliseconds(2);
  EXPECT_EQ(Time::Zero() + Time::Delta::FromMilliseconds(1),
            time - Time::Delta::FromMilliseconds(1));
}

TEST(TimeTest, Max) {
  Time time_1 = Time::Zero() + Time::Delta::FromMilliseconds(1);
  Time time_2 = Time::Zero() + Time::Delta::FromMilliseconds(2);

  EXPECT_EQ(time_2, std::max(time_1, time_2));
}

TEST(TimeTest, LE) {
  const Time zero = Time::Zero();
  const Time one = zero + Time::Delta::FromSeconds(1);
  EXPECT_TRUE(zero <= zero);
  EXPECT_TRUE(zero <= one);
  EXPECT_TRUE(one <= one);
  EXPECT_FALSE(one <= zero);
}

TEST(WallTimeTest, Zero) {
  EXPECT_TRUE(WallTime::Zero().IsZero());
  EXPECT_FALSE(WallTime::FromUNIXSeconds(1).IsZero());
}

TEST(WallTimeTest, FromTo) {
  EXPECT_EQ(WallTime::FromUNIXSeconds(1),
            WallTime::FromUNIXMicroseconds(1000000));
  EXPECT_EQ(1, WallTime::FromUNIXMicroseconds(1000000).ToUNIXSeconds());
  EXPECT_EQ(2000000, WallTime::FromUNIXSeconds(2).ToUNIXMicroseconds());
}

TEST(WallTimeTest, Compare) {
  EXPECT_TRUE(
      WallTime::FromUNIXMicroseconds(1).IsBefore(WallTime::FromUNIXSeconds(1)));
  EXPECT_TRUE(
      WallTime::FromUNIXSeconds(1).IsAfter(WallTime::FromUNIXMicroseconds(1)));
  EXPECT_FALSE(WallTime::FromUNIXMicroseconds(1000000).IsAfter(
      WallTime::FromUNIXSeconds(1)));
  EXPECT_FALSE(WallTime::FromUNIXMicroseconds(1000000).IsBefore(
      WallTime::FromUNIXSeconds(1)));
  EXPECT_TRUE(WallTime::FromUNIXSeconds(1) ==
              WallTime::FromUNIXMicroseconds(1000000));
  EXPECT_FALSE(WallTime::FromUNIXSeconds(1) ==
               WallTime::FromUNIXMicroseconds(1));
}

TEST(WallTimeTest, Add) {
  EXPECT_EQ(WallTime::FromUNIXMicroseconds(2000000),
            WallTime::Zero().Add(Time::Delta::FromSeconds(2)));
}

TEST(WallTimeTest, Subtract) {
  EXPECT_EQ(WallTime::FromUNIXMicroseconds(1000),
            WallTime::FromUNIXMicroseconds(2000).Subtract(
                Time::Delta::FromMilliseconds(1)));
  EXPECT_EQ(
      Time::Delta::FromSeconds(1),
      WallTime::FromUNIXSeconds(2) - WallTime::FromUNIXMicroseconds(1000000));
  EXPECT_EQ(WallTime::Zero(),
            WallTime::Zero().Subtract(Time::Delta::FromSeconds(1)));
  EXPECT_EQ(Time::Delta::FromSeconds(-1),
            WallTime::Zero() - WallTime::FromUNIXSeconds(1));
}

}  // namespace iomgr

int main(int argc, char** argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}