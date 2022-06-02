#include "util/file_op.h"

#include <string>

#include "glog/logging.h"
#include "gtest/gtest.h"

namespace iomgr {

TEST(FileOpTest, Pipe) {
  int fds[2];
  CHECK(FileOp::pipe(fds, true).ok());

  const std::string testw = "hello world";
  StatusOr<int> wrote = FileOp::write(fds[1], testw.data(), testw.size());
  EXPECT_TRUE(wrote.ok());
  EXPECT_EQ(testw.size(), wrote.value());

  std::string testr(100, 0);
  StatusOr<int> read = FileOp::read(fds[0], &testr[0], testr.size());
  EXPECT_TRUE(read.ok());
  EXPECT_EQ(testw.size(), read.value());
  testr.resize(testw.size());
  EXPECT_EQ(testw, testr);

  EXPECT_EQ(0, ::close(fds[0]));
  EXPECT_EQ(0, ::close(fds[1]));
}

TEST(FileOpTest, Eventfd) {
  StatusOr<int> eventfd = FileOp::eventfd(0, true);
  EXPECT_TRUE(eventfd.ok());
  EXPECT_GT(eventfd.value(), -1);
  int fd = eventfd.value();
  uint64_t value = 1;
  EXPECT_TRUE(FileOp::eventfd_write(fd, value).ok());
  uint64_t buf = 0;
  EXPECT_TRUE(FileOp::eventfd_read(fd, &buf).ok());
  EXPECT_EQ(value, buf);
}

}  // namespace iomgr

int main(int argc, char** argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}