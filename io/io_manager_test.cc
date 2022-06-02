#include "io/io_manager.h"

#include "glog/logging.h"
#include "gtest/gtest.h"
#include "util/file_op.h"
#include "util/notification.h"

namespace iomgr {

class IOManagerTest : public testing::Test {
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

  void CheckRemoved(IOManager* iomgr) {
    auto fd_i =
        iomgr->fd_controllers_.find(IOManager::FDAndControllers(eventfd()));
    auto* fd_ctrl = const_cast<IOManager::FDAndControllers*>(&*fd_i);
    DCHECK_EQ(0, fd_ctrl->mode);
  }

 private:
  ScopedFD eventfd_;
};

class ReadWatcher : public IOWatcher {
 public:
  explicit ReadWatcher(Notification* notification)
      : notification_(CHECK_NOTNULL(notification)) {}

  ~ReadWatcher() = default;

  void OnFileReadable(int) { notification_->Notify(); }
  void OnFileWritable(int) { DCHECK(false); }

 protected:
  Notification* notification_;
};

TEST_F(IOManagerTest, Get) { IOManager* iomgr = IOManager::Get(); }

TEST_F(IOManagerTest, WatchFileDescriptor) {
  IOManager* iomgr = IOManager::Get();
  DCHECK_NE(-1, eventfd());

  Notification notification;
  ReadWatcher watcher(&notification);
  IOWatcher::Controller controller;
  DCHECK(iomgr->WatchFileDescriptor(eventfd(), IOWatcher::kWatchRead, &watcher,
                                    &controller));
  TriggerReadable();
  notification.WaitForNotification();
}

TEST_F(IOManagerTest, StopWatchingFileDescriptor) {
  IOManager* iomgr = IOManager::Get();
  DCHECK_NE(-1, eventfd());

  Notification notification;
  ReadWatcher watcher(&notification);
  IOWatcher::Controller controller;
  DCHECK(iomgr->WatchFileDescriptor(eventfd(), IOWatcher::kWatchRead, &watcher,
                                    &controller));
  DCHECK(iomgr->StopWatchingFileDescriptor(&controller));
  CheckRemoved(iomgr);
}

TEST_F(IOManagerTest, StopWatchingFileDescriptorAgain) {
  IOManager* iomgr = IOManager::Get();
  DCHECK_NE(-1, eventfd());

  Notification notification;
  ReadWatcher watcher(&notification);
  IOWatcher::Controller controller;
  DCHECK(iomgr->WatchFileDescriptor(eventfd(), IOWatcher::kWatchRead, &watcher,
                                    &controller));
  DCHECK(iomgr->StopWatchingFileDescriptor(&controller));
  CheckRemoved(iomgr);
  DCHECK(iomgr->StopWatchingFileDescriptor(&controller));
  CheckRemoved(iomgr);
}

}  // namespace iomgr

int main(int argc, char** argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}