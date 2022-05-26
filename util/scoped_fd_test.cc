#include "util/scoped_fd.h"

#include <fcntl.h>
#include <glog/logging.h>
#include <gtest/gtest.h>

namespace iomgr {

TEST(ScopedFD, Constructor) {
  ScopedFD fd;
  EXPECT_EQ(-1, fd);
}

TEST(ScopedFD, Destroctor) {
  int fd = -1;
  {
    ScopedFD fd2(::open(".tmp1", O_WRONLY | O_CREAT, 0600));
    EXPECT_GT(fd2, 0);
    fd = fd2;
  }

  char dummy = 0;
  EXPECT_EQ(-1L, ::write(fd, &dummy, 1));
  EXPECT_EQ(EBADF, errno);
}

TEST(ScopedFD, reset) {
  ScopedFD fd1(::open(".tmp1", O_WRONLY | O_CREAT, 0600));
  EXPECT_GT(fd1, 0);
  int raw_fd1 = fd1;

  const int raw_fd2 = ::open(".tmp2", O_WRONLY | O_CREAT, 0600);
  fd1.reset(raw_fd2);
  EXPECT_GT(fd1, 0);
  char dummy = 0;
  EXPECT_EQ(-1L, ::write(raw_fd1, &dummy, 1));
  EXPECT_EQ(EBADF, errno);

  fd1.reset(-1);
  EXPECT_EQ(-1L, ::write(raw_fd2, &dummy, 1));
  EXPECT_EQ(EBADF, errno);
}

TEST(ScopedFD, release) {
  const int raw_fd = ::open(".tmp1", O_WRONLY | O_CREAT, 0600);
  ScopedFD fd(raw_fd);
  EXPECT_GT(fd, 0);

  EXPECT_EQ(raw_fd, fd.release());
  EXPECT_EQ(-1, fd);
  ::close(raw_fd);
}

}  // namespace iomgr

int main(int argc, char** argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
