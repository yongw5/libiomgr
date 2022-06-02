#include "util/notification.h"

#include "glog/logging.h"
#include "gtest/gtest.h"

namespace iomgr {

TEST(NotificationTest, BasicTests) {
  Notification notifier;

  ASSERT_FALSE(notifier.HasBeenNotified());
  notifier.Notify();
  notifier.WaitForNotification();
  ASSERT_TRUE(notifier.HasBeenNotified());
}

TEST(NotificationDeathTest, NotifyOnce) {
  Notification notifier;

  ASSERT_FALSE(notifier.HasBeenNotified());
  notifier.Notify();
  ASSERT_TRUE(notifier.HasBeenNotified());
  EXPECT_DEATH(notifier.Notify(), "Check failed: !notified_");
}

}  // namespace iomgr

int main(int argc, char** argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}