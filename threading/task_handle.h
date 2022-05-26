#ifndef LIBIOMGR_THREADING_TASK_HANDLE_H_
#define LIBIOMGR_THREADING_TASK_HANDLE_H_

#include "iomgr/ref_counted.h"

namespace iomgr {

class TaskHandle {
 public:
  class Delegate : public RefCounted<Delegate> {
   public:
    virtual void CancelTask() = 0;
    virtual void WaitIfRunning() = 0;

   protected:
    friend class RefCounted<Delegate>;

    virtual ~Delegate() = default;
  };

  TaskHandle();
  explicit TaskHandle(RefPtr<Delegate> delegate);

  TaskHandle(TaskHandle&&);
  TaskHandle& operator=(TaskHandle&&);

  ~TaskHandle();

  void CancelTask();
  void WaitIfRunning();

 private:
  RefPtr<Delegate> delegate_;
};

}  // namespace iomgr

#endif  // LIBIOMGR_THREADING_TASK_HANDLE_H_