#include "iomgr/timer.h"

#include "glog/logging.h"
#include "threading/task_handle.h"
#include "timer/timer_manager.h"

namespace iomgr {

const uint32_t kInvalidIndex = 0xffffffffu;

/// Timer
Timer::Timer()
    : deadline_(Time::Zero()),
      pending_(false),
      heap_index_(kInvalidIndex),
      closure_(),
      controller_(nullptr) {
  entry_.le_next = nullptr;
  entry_.le_prev = nullptr;
}

Timer::~Timer() { DCHECK(!pending_); }

void Timer::Start(Time::Delta delay, std::function<void()> closure,
                  Timer::Controller* controller) {
  DCHECK(controller);

  TimerManager::Get()->TimerInit(delay, closure, controller);
}

/// Timer::Controller
Timer::Controller::Controller() : timer_(), scheduled_() {}

Timer::Controller::~Controller() = default;

void Timer::Controller::Cancel() { TimerManager::Get()->TimerCancel(this); }

}  // namespace iomgr
