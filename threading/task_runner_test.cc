#include "threading/task_runner.h"

#include <atomic>
#include <memory>

#include "glog/logging.h"
#include "gtest/gtest.h"
#include "threading/task_runner_task.h"

namespace iomgr {

class TaskRunnerTest : public testing::Test {
 protected:
  class TaskWrapper {
   public:
    explicit TaskWrapper(std::function<void()> closure) : task_(closure) {}
    void Run() { task_.Run(); }
    void Cancel() { task_.CancelTask(); }
    bool pending() { return task_.task_state_ == TaskRunner::Task::kPending; }
    bool canceled() { return task_.task_state_ == TaskRunner::Task::kCanceled; }
    bool completed() {
      return task_.task_state_ == TaskRunner::Task::kCompleted;
    }

   private:
    TaskRunner::Task task_;
  };

  struct TaskRunnerDeleter {
    void operator()(TaskRunner* runner) const { delete runner; }
  };

  std::unique_ptr<TaskRunner, TaskRunnerDeleter> CreateTaskRunner() {
    return std::unique_ptr<TaskRunner, TaskRunnerDeleter>(new TaskRunner(0));
  }
  void RunTasks(TaskRunner* task) { task->RunTaskForTEST(); }
};

class Functor {
 public:
  static void RunOnce() { counter_.fetch_add(1); }

  static int count() { return counter_.load(); }
  static void reset() { counter_.store(0); }

 private:
  static std::atomic<int> counter_;
};
std::atomic<int> Functor::counter_;

TEST_F(TaskRunnerTest, Task) {
  TaskWrapper task(Functor::RunOnce);
  EXPECT_TRUE(task.pending());
  EXPECT_FALSE(task.canceled());
  EXPECT_FALSE(task.completed());
}

TEST_F(TaskRunnerTest, TaskRun) {
  Functor::reset();
  TaskWrapper task(Functor::RunOnce);
  EXPECT_TRUE(task.pending());
  EXPECT_EQ(0, Functor::count());

  task.Run();

  EXPECT_TRUE(task.completed());
  EXPECT_EQ(1, Functor::count());
}

TEST_F(TaskRunnerTest, TaskCancel) {
  Functor::reset();
  TaskWrapper task(Functor::RunOnce);
  EXPECT_TRUE(task.pending());
  EXPECT_EQ(0, Functor::count());

  task.Cancel();

  EXPECT_TRUE(task.canceled());
  EXPECT_EQ(0, Functor::count());
}

TEST_F(TaskRunnerTest, PostTask) {
  Functor::reset();
  auto runner = CreateTaskRunner();
  runner->PostTask(Functor::RunOnce);
  EXPECT_EQ(0, Functor::count());
  RunTasks(runner.get());
  EXPECT_EQ(1, Functor::count());
}

TEST_F(TaskRunnerTest, TaskHandle) {
  TaskHandle handle;
  handle.CancelTask();
  handle.WaitIfRunning();
}

TEST_F(TaskRunnerTest, TaskHandle2) {
  Functor::reset();
  auto runner = CreateTaskRunner();
  TaskHandle handle = runner->PostTask(Functor::RunOnce);
  EXPECT_EQ(0, Functor::count());
  handle.CancelTask();
  RunTasks(runner.get());
  EXPECT_EQ(0, Functor::count());
}

TEST_F(TaskRunnerTest, Get) {
  Functor::reset();
  const int kOps = 100;
  TaskRunner* runner = TaskRunner::Get();
  for (int i = 0; i < kOps; ++i) {
    runner->PostTask(Functor::RunOnce);
  }
  CurrentThread::SleepFor(Time::Delta::FromMilliseconds(100));
  EXPECT_EQ(kOps, Functor::count());
}

}  // namespace iomgr

int main(int argc, char** argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}