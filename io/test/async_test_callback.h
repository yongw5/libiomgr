#ifndef LIBIOMGR_IO_TEST_ASYNC_TEST_CALLBACK_H_
#define LIBIOMGR_IO_TEST_ASYNC_TEST_CALLBACK_H_

#include <functional>

#include "iomgr/status.h"
#include "iomgr/statusor.h"
#include "util/notification.h"
#include "util/sync.h"

namespace iomgr {

template <class Result>
class AsyncResultCheckHelper {
 public:
  static bool IsTryAgain(Result result) {
    DCHECK(false) << "NOTOUCH";
    return false;
  }
};

template <>
class AsyncResultCheckHelper<Status> {
 public:
  static bool IsTryAgain(Status result) { return result.IsTryAgain(); }
};

template <>
class AsyncResultCheckHelper<StatusOr<int>> {
 public:
  static bool IsTryAgain(StatusOr<int> result) {
    return result.status().IsTryAgain();
  }
};

template <class Result,
          typename AsyncResultChecker = AsyncResultCheckHelper<Result>>
class AsyncTestCallback {
 public:
  using Callback = std::function<void(Result)>;

  AsyncTestCallback() = default;
  ~AsyncTestCallback() = default;

  AsyncTestCallback(const AsyncTestCallback&) = delete;
  AsyncTestCallback& operator=(const AsyncTestCallback&) = delete;

  Result WaitForResult() {
    notification_.WaitForNotification();
    return std::move(result_);
  }

  Result GetResult(Result result) {
    if (!AsyncResultChecker::IsTryAgain(result)) {
      return std::move(result);
    }
    return WaitForResult();
  }

  Callback callback() {
    return std::bind(&AsyncTestCallback::SetResult, this,
                     std::placeholders::_1);
  }

 private:
  void SetResult(Result result) {
    result_ = std::move(result);
    notification_.Notify();
  }

  Notification notification_;
  Result result_;
};

using StatusResultCallback = AsyncTestCallback<Status>;
using StatusOrResultCallback = AsyncTestCallback<StatusOr<int>>;

}  // namespace iomgr

#endif  // LIBIOMGR_IO_TEST_ASYNC_TEST_CALLBACK_H_