#include "threading/thread.h"

#include "glog/logging.h"
#include "gtest/gtest.h"

namespace iomgr {

class DoNothingThread : public Thread {
 private:
  void ThreadEntry() override {}
};

class LoopThread : public Thread {
 private:
  void ThreadEntry() override {
    while (!stop_) {
      // do_nothing
    }
  }
  void TerminateThread() override { stop_ = true; }
  bool stop_{false};
};

TEST(ThreadTest, Stop) {
  DoNothingThread thread;
  EXPECT_FALSE(thread.started());
  thread.StopThread();
}

TEST(ThreadTest, Stop2) {
  DoNothingThread thread;
  EXPECT_FALSE(thread.started());
  thread.StopThread();
  thread.StopThread();
}

TEST(ThreadTest, StartAndStop) {
  DoNothingThread thread;
  EXPECT_FALSE(thread.started());
  thread.StartThread();
  EXPECT_TRUE(thread.started());
  thread.StopThread();
  EXPECT_FALSE(thread.started());
}

TEST(ThreadTest, StartAndStop2) {
  LoopThread thread;
  EXPECT_FALSE(thread.started());
  thread.StartThread();
  EXPECT_TRUE(thread.started());
  thread.StopThread();
  EXPECT_FALSE(thread.started());
}

}  // namespace iomgr

int main(int argc, char** argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}