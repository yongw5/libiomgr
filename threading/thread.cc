#include "threading/thread.h"

#include "glog/logging.h"

namespace iomgr {

const Thread::Id Thread::invalid_id = Thread::Id();

Thread::~Thread() { StopThread(); }

void Thread::StartThread() {
  CHECK(!started()) << "Threads should persist and not be restarted.";

  try {
    thread_.reset(new std::thread(&Thread::ThreadEntry, this));
  } catch (std::exception& e) {
    LOG(FATAL) << "Thread exception: " << e.what();
  }
}

void Thread::StopThread() {
  if (started()) {
    TerminateThread();
    try {
      thread_->join();
    } catch (std::exception& e) {
      LOG(FATAL) << "Thread exception: " << e.what();
    }
  }
}
bool Thread::started() const { return thread_ && thread_->joinable(); }

Thread::Id Thread::get_id() const {
  Id ret;
  if (started()) {
    ret = thread_->get_id();
  }
  return ret;
}

void CurrentThread::SleepFor(Time::Delta duration) {
  DCHECK_GT(duration, Time::Delta::Zero());

  std::this_thread::sleep_for(
      std::chrono::duration<int64_t, std::micro>(duration.ToMicroseconds()));
}

void CurrentThread::SleepUntil(Time timepoint) {
  Time now = Time::Now();
  if (now < timepoint) {
    SleepFor(timepoint - now);
  }
}

Thread::Id CurrentThread::get_id() { return std::this_thread::get_id(); }

}  // namespace iomgr
