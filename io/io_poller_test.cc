#include "io/io_poller.h"

#include "glog/logging.h"
#include "gtest/gtest.h"
#include "util/file_op.h"

#define kWatchRead 1

namespace iomgr {

const int kMaxPollSize = 5;

TEST(IOPollerTest, Constructor) { IOPoller poller(kMaxPollSize); }

TEST(IOPollerTest, Poll) {
  IOPoller poller(kMaxPollSize);
  Time::Delta timeout = Time::Delta::Zero();
  std::vector<IOEvent> io_events;
  poller.Poll(timeout, &io_events);
}

TEST(IOPollerTest, Poll2) {
  IOPoller poller(kMaxPollSize);
  Time::Delta timeout = Time::Delta::FromMilliseconds(1);
  std::vector<IOEvent> io_events;
  poller.Poll(timeout, &io_events);
}

TEST(IOPollerTest, AddAndPoll) {
  IOPoller poller(kMaxPollSize);
  Time::Delta timeout = Time::Delta::FromMilliseconds(-1);

  StatusOr<int> eventfd = FileOp::eventfd(0, true);
  EXPECT_TRUE(poller.AddFd(eventfd.value(), kWatchRead, &eventfd).ok());
  FileOp::eventfd_write(eventfd.value(), 1);

  std::vector<IOEvent> io_events;
  poller.Poll(timeout, &io_events);
  EXPECT_EQ(1, io_events.size());
  EXPECT_EQ(&eventfd, io_events[0].data);
  EXPECT_EQ(kWatchRead, io_events[0].ready);

  EXPECT_TRUE(poller.RemoveFd(eventfd.value()).ok());
  FileOp::close(eventfd.value());
}

}  // namespace iomgr

int main(int argc, char** argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}