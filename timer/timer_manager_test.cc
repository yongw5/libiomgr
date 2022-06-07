#include "timer/timer_manager.h"

#include <glog/logging.h>
#include <gtest/gtest.h>
#include <math.h>

#include "iomgr/time.h"
#include "iomgr/timer.h"
#include "util/notification.h"

namespace iomgr {

void SimpleClosure() {}

TEST(TimerManager, Ctor) { TimerManager mgr; }

TEST(TimerManager, TimerInit) {
  TimerManager mgr;
  Timer::Controller controller;
  EXPECT_FALSE(controller.pending());

  mgr.TimerInit(Time::Delta::FromMicroseconds(10), std::bind(SimpleClosure),
                &controller);
  EXPECT_TRUE(controller.pending());
  mgr.TimerCancel(&controller);
  EXPECT_FALSE(controller.pending());
}

TEST(TimerController, Ctor) {
  Timer::Controller controller;
  EXPECT_FALSE(controller.pending());
}

TEST(TimerController, Cancel) {
  Timer::Controller controller;
  controller.Cancel();
}

void Notify(Notification* notification) {
  DCHECK(notification);
  notification->Notify();
}

TEST(Timer, Start) {
  Notification notification;
  Timer::Controller controller;
  Timer::Start(Time::Delta::FromMilliseconds(100),
               std::bind(Notify, &notification), &controller);
  notification.WaitForNotification();
}

}  // namespace iomgr

int main(int argc, char** argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
