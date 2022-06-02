#include <sys/socket.h>

#include "glog/logging.h"
#include "gtest/gtest.h"
#include "io/io_manager.h"
#include "util/file_op.h"
#include "util/notification.h"

namespace iomgr {

class IOWatcherTest : public testing::Test {
 protected:
  void SetUp() {
    StatusOr<int> eventfd = FileOp::eventfd(0, /*non_blocking*/ true);
    CHECK(eventfd.ok());
    eventfd_.reset(eventfd.value());
  }

  void TriggerReadable() {
    uint64_t data = 1;
    StatusOr<int> written = FileOp::write(eventfd_, &data, sizeof(data));
    CHECK(written.ok());
    CHECK_EQ(sizeof(data), written.value());
  }

  int eventfd() const { return eventfd_; }

 private:
  ScopedFD eventfd_;
};

class DeleteWatcher : public IOWatcher {
 public:
  DeleteWatcher(IOWatcher::Controller* controller, Notification* notification)
      : controller_(CHECK_NOTNULL(controller)),
        notification_(CHECK_NOTNULL(notification)) {}

  ~DeleteWatcher() { CHECK(!controller_); }

  void OnFileReadable(int) override {
    CHECK(controller_);
    delete controller_;
    controller_ = nullptr;
    notification_->Notify();
  }

  void OnFileWritable(int) {}

 private:
  IOWatcher::Controller* controller_;
  Notification* notification_;
};

TEST_F(IOWatcherTest, DeleteWatcher) {
  Notification notification;
  IOWatcher::Controller* controller = new IOWatcher::Controller;
  DeleteWatcher watcher(controller, &notification);

  DCHECK(IOWatcher::WatchFileDescriptor(eventfd(), IOWatcher::kWatchRead,
                                        &watcher, controller));
  TriggerReadable();
  notification.WaitForNotification();
}

class StopWatcher : public IOWatcher {
 public:
  StopWatcher(IOWatcher::Controller* controller, Notification* notification)
      : controller_(CHECK_NOTNULL(controller)),
        notification_(CHECK_NOTNULL(notification)) {}

  ~StopWatcher() = default;

  void OnFileReadable(int) override {
    controller_->StopWatching();
    notification_->Notify();
  }

  void OnFileWritable(int) {}

 private:
  IOWatcher::Controller* controller_;
  Notification* notification_;
};

TEST_F(IOWatcherTest, StopWatcher) {
  Notification notification;
  IOWatcher::Controller controller;
  StopWatcher watcher(&controller, &notification);

  DCHECK(IOWatcher::WatchFileDescriptor(eventfd(), IOWatcher::kWatchRead,
                                        &watcher, &controller));
  TriggerReadable();
  notification.WaitForNotification();
}

class NestedWatchWatcher : public IOWatcher {
 public:
  explicit NestedWatchWatcher(IOWatcher::Controller* controller,
                              Notification* notification)
      : controller_(CHECK_NOTNULL(controller)),
        notification_(CHECK_NOTNULL(notification)) {}

  ~NestedWatchWatcher() = default;

  void OnFileReadable(int fd) {
    CHECK(controller_->StopWatching());
    notification_->Notify();
  }

  void OnFileWritable(int fd) {
    CHECK(controller_->StopWatching());
    CHECK(IOWatcher::WatchFileDescriptor(fd, IOWatcher::kWatchRead, this,
                                         controller_));
  }

 private:
  IOWatcher::Controller* controller_;
  Notification* notification_;
};

TEST_F(IOWatcherTest, NestedWatchWatcher) {
  Notification notification;
  IOWatcher::Controller controller;
  NestedWatchWatcher watcher(&controller, &notification);

  DCHECK(IOWatcher::WatchFileDescriptor(eventfd(), IOWatcher::kWatchWrite,
                                        &watcher, &controller));
  TriggerReadable();
  notification.WaitForNotification();
}

// A watcher that owns its controller and will either delete itself or stop
// watching the file after observing the specified event type
class ReadWriteWatcher : public IOWatcher {
 public:
  enum Action {
    kStopWatching,
    kDelete,
  };
  enum ActWhen {
    kActWhenRead,
    kActWhenWrite,
  };

  ReadWriteWatcher(Action action, ActWhen when, Notification* notification)
      : action_(action),
        when_(when),
        controller_(),
        notification_(CHECK_NOTNULL(notification)) {}

  ReadWriteWatcher(const ReadWriteWatcher&) = delete;
  ReadWriteWatcher& operator=(const ReadWriteWatcher&) = delete;

  void OnFileReadable(int fd) override {
    if (when_ == kActWhenRead) {
      DoAction();
    } else {
      char c;
      StatusOr<int> read_result = FileOp::read(fd, &c, 1);
      EXPECT_TRUE(read_result.ok());
      EXPECT_EQ(1, read_result.value());
    }
  }

  void OnFileWritable(int fd) override {
    if (when_ == kActWhenWrite) {
      DoAction();
    } else {
      char c = '0';
      StatusOr<int> write_result = FileOp::write(fd, &c, 1);
      EXPECT_TRUE(write_result.ok());
      EXPECT_EQ(1, write_result.value());
    }
  }

  IOWatcher::Controller* controller() { return &controller_; }

 private:
  void DoAction() {
    Notification* notification = notification_;
    if (action_ == kDelete) {
      delete this;
    } else if (action_ == kStopWatching) {
      controller_.StopWatching();
    }
    notification->Notify();
  }

  Action action_;
  ActWhen when_;
  IOWatcher::Controller controller_;
  Notification* notification_;
};

class IOWatchControllerReadAndWriteTest
    : public testing::TestWithParam<ReadWriteWatcher::Action> {
 protected:
  static bool CreateSocketPair(int& one, int& two) {
    int fds[2];
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, fds) == -1) {
      return false;
    }
    one = fds[0];
    two = fds[1];
    return true;
  }
  static void TriggerReadable(int fd) {
    char c = '\0';
    StatusOr<int> write_result = FileOp::write(fd, &c, 1);
    EXPECT_TRUE(write_result.ok());
    EXPECT_EQ(1, write_result.value());
  }
};

INSTANTIATE_TEST_CASE_P(StopWachingOrDelete, IOWatchControllerReadAndWriteTest,
                        testing::Values(ReadWriteWatcher::kStopWatching,
                                        ReadWriteWatcher::kDelete));

// Test deleting or stopping watch after a read event for watcher that is
// registered for both read and write
TEST_P(IOWatchControllerReadAndWriteTest, AfterRead) {
  int one, two;
  CHECK(CreateSocketPair(one, two));

  Notification notification;
  ReadWriteWatcher* watcher = new ReadWriteWatcher(
      GetParam(), ReadWriteWatcher::kActWhenRead, &notification);

  // Trigger a read event on |one| by writing to |two|
  TriggerReadable(two);

  // The triggered read will cause the watcher to run. |one| would also be
  // immediately available for writing, so this should not cause a
  // use-after-free on the |watcher|.
  IOWatcher::WatchFileDescriptor(one, IOWatcher::kWatchReadWrite, watcher,
                                 watcher->controller());
  notification.WaitForNotification();

  if (GetParam() == ReadWriteWatcher::kStopWatching) {
    delete watcher;
  }
}

// Test deleting or stopping watch after a read event for watcher that is
// registered for both read and write
TEST_P(IOWatchControllerReadAndWriteTest, AfterWrite) {
  int one, two;
  CHECK(CreateSocketPair(one, two));

  Notification notification;
  ReadWriteWatcher* watcher = new ReadWriteWatcher(
      GetParam(), ReadWriteWatcher::kActWhenRead, &notification);

  // Trigger two read events on |one| by writing to |two|. Because each read
  // event only reads one char,
  TriggerReadable(two);

  // The triggered read will cause the watcher to run. |one| would also be
  // immediately available for writing, so this should not cause a
  // use-after-free on the |watcher|.
  IOWatcher::WatchFileDescriptor(one, IOWatcher::kWatchReadWrite, watcher,
                                 watcher->controller());
  notification.WaitForNotification();

  if (GetParam() == ReadWriteWatcher::kStopWatching) {
    delete watcher;
  }
}

}  // namespace iomgr

int main(int argc, char** argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}