// Copyright (c) 2018 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#include "iomgr/statusor.h"

#include <gtest/gtest.h>

#include <memory>
#include <type_traits>

namespace iomgr {

TEST(StatusOr, ElementType) {
  static_assert(std::is_same<StatusOr<int>::value_type, int>(), "");
  static_assert(std::is_same<StatusOr<char>::value_type, char>(), "");
}

TEST(StatusOr, ValueAccessor) {
  const int kIntValue = 110;
  StatusOr<int> status_or(kIntValue);
  EXPECT_EQ(kIntValue, status_or.value());
  EXPECT_EQ(kIntValue, std::move(status_or).value());
}

TEST(StatusOr, BadValueAccess) {
  const int kIntValue = 110;
  StatusOr<int> status_or(Status::NotFound(""));
  EXPECT_FALSE(status_or.ok());
  EXPECT_DEATH(status_or.value(), "NotFound");
}

TEST(StatusOr, AssignmentStatusOk) {
  // Copy assignment
  {
    const int kIntValue = 17;
    const auto p = std::make_shared<int>(kIntValue);
    StatusOr<std::shared_ptr<int>> source(p);

    const auto p2 = std::make_shared<int>(kIntValue * 2);
    StatusOr<std::shared_ptr<int>> target(p2);
    target = source;

    EXPECT_TRUE(target.ok());
    EXPECT_EQ(p, target.value());

    EXPECT_TRUE(source.ok());
    EXPECT_EQ(p, source.value());
  }

  // Move assignment
  {
    const int kIntValue = 17;
    const auto p = std::make_shared<int>(kIntValue);
    StatusOr<std::shared_ptr<int>> source(p);

    const auto p2 = std::make_shared<int>(kIntValue * 2);
    StatusOr<std::shared_ptr<int>> target(p2);
    target = std::move(source);

    EXPECT_TRUE(target.ok());
    EXPECT_EQ(p, target.value());
  }
}

TEST(StatusOr, AssignmentStatusNotOk) {
  // Copy assignment
  {
    const int kIntValue = 17;
    const auto p = std::make_shared<int>(kIntValue);
    StatusOr<std::shared_ptr<int>> source(Status::NotFound(""));

    const auto p2 = std::make_shared<int>(kIntValue * 2);
    StatusOr<std::shared_ptr<int>> target(p2);
    target = source;

    EXPECT_FALSE(target.ok());
    EXPECT_DEATH(target.value(), "NotFound");

    EXPECT_FALSE(source.ok());
    EXPECT_DEATH(source.value(), "NotFound");
  }

  // Move assignment
  {
    const int kIntValue = 17;
    const auto p = std::make_shared<int>(kIntValue);
    StatusOr<std::shared_ptr<int>> source(Status::NotFound(""));

    const auto p2 = std::make_shared<int>(kIntValue * 2);
    StatusOr<std::shared_ptr<int>> target(p2);
    target = std::move(source);

    EXPECT_FALSE(target.ok());
    EXPECT_DEATH(target.value(), "NotFound");
  }
}

StatusOr<std::unique_ptr<int>> ReturnUniquePtr(const int value = 0) {
  return StatusOr<std::unique_ptr<int>>(std::unique_ptr<int>(new int(value)));
}

TEST(StatusOr, TestMoveOnlyInitialize) {
  StatusOr<std::unique_ptr<int>> thing(ReturnUniquePtr());
  EXPECT_TRUE(thing.ok());
  EXPECT_EQ(0, *thing.value());
  int* preview = thing.value().get();

  thing = ReturnUniquePtr();
  EXPECT_TRUE(thing.ok());
  EXPECT_EQ(0, *thing.value());
  EXPECT_NE(preview, thing.value().get());
}

TEST(StatusOr, TestMoveOnlyValueExtraction) {
  StatusOr<std::unique_ptr<int>> thing(ReturnUniquePtr());
  EXPECT_TRUE(thing.ok());
  std::unique_ptr<int> ptr = std::move(thing).value();
  EXPECT_EQ(0, *ptr);

  thing = StatusOr<std::unique_ptr<int>>(std::move(ptr));
  ptr = std::move(thing.value());
  EXPECT_EQ(0, *ptr);
}

}  // namespace iomgr

int main(int argc, char** argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
