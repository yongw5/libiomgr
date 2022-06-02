// Copyright (c) 2018 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#include "iomgr/status.h"

#include "glog/logging.h"
#include "gtest/gtest.h"

namespace iomgr {

#define TEST_CASE(token, msg)                                 \
  do {                                                        \
    Status status = Status::token(#msg);                      \
    Status status2 = status;                                  \
    EXPECT_TRUE(status2.Is##token());                         \
    CHECK_EQ(#token ": " #msg, status2.ToString());           \
                                                              \
    Status status3 = Status::token(#msg, #msg);               \
    Status status4 = std::move(status3);                      \
    EXPECT_TRUE(status4.Is##token());                         \
    CHECK_EQ(#token ": " #msg ": " #msg, status4.ToString()); \
  } while (0)

TEST(Status, Constructor) {
  Status status;
  EXPECT_TRUE(status.ok());
}

TEST(Status, Constructor2) {
  Status status = Status::NoPermission("no permission");
  EXPECT_TRUE(status.IsNoPermission());

  Status status2 = status;
  EXPECT_TRUE(status2.IsNoPermission());
}

TEST(Status, MoveConstructor) {
  Status ok = Status::OK();
  Status ok2 = std::move(ok);

  EXPECT_TRUE(ok2.ok());
}

TEST(Status, SelfMove) {
  Status self_moved = Status::IOError("custom IOError status message");

  // Needed to bypass compiler warning about explicit move-assignment.
  Status& self_moved_reference = self_moved;
  self_moved_reference = std::move(self_moved);
}

TEST(Status, Assignment) {
  Status status, status2 = Status::NoPermission("no permission");
  EXPECT_TRUE(status.ok());
  EXPECT_TRUE(status2.IsNoPermission());

  status = status2;
  EXPECT_TRUE(status.IsNoPermission());
}

TEST(Status, Assignment2) {
  TEST_CASE(Unknown, "unkown");
  TEST_CASE(InvalidArg, "invalid argument");
  TEST_CASE(NotFound, "not found");
  TEST_CASE(NotSupported, "not supported");
  TEST_CASE(Corruption, "corruption");
  TEST_CASE(IOError, "io error");
  TEST_CASE(TryAgain, "try again");
  TEST_CASE(Unimplemented, "unimplemented");
  TEST_CASE(NoPermission, "no permission");
  TEST_CASE(OutOfMemory, "out of memory");
  TEST_CASE(OutOfRange, "out of range");
  TEST_CASE(InUse, "in use");
  TEST_CASE(Timeout, "timeout");
  TEST_CASE(Internal, "internal");
}

}  // namespace iomgr

int main(int argc, char** argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
