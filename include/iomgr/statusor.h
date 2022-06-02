#ifndef LIBIOMGR_INCLUDE_STATUSOR_H_
#define LIBIOMGR_INCLUDE_STATUSOR_H_

#include "glog/logging.h"
#include "iomgr/export.h"
#include "iomgr/status.h"

namespace iomgr {

template <typename T>
class IOMGR_EXPORT StatusOr {
 public:
  // StatusOr<T>::value_type
  typedef T value_type;

  StatusOr() : status_(Status::OK()), value_(0){};
  ~StatusOr() {
    if (ok()) {
      value_.~T();
    }
  }
  // Contructs a new |StatusOr| with an |OK| status.
  StatusOr(const T& value) : status_(Status::OK()), value_(value) {}
  StatusOr(T&& value) : status_(Status::OK()), value_(std::move(value)) {}

  // Constructs a new |StatusOr| with non-ok status.
  StatusOr(const Status& status);
  StatusOr(Status&& status);

  StatusOr(const StatusOr& rhs);
  StatusOr& operator=(const StatusOr& rhs);
  StatusOr(StatusOr&& rhs) noexcept;
  StatusOr& operator=(StatusOr&& rhs) noexcept;

  bool ok() const { return status_.ok(); }
  const Status& status() const& { return status_; }
  Status status() && { return std::move(status_); }

  const T& value() const&;
  T& value() &;
  T&& value() &&;

 private:
  template <typename... Args>
  void MakeValue(void* ptr, Args... arg) {
    new (ptr) T(std::forward<Args>(arg)...);
  }
  // |status_| will always be active after te constructor.
  Status status_;
  // data_ is active iff status_.ok() == true
  struct Dummy {};
  union {
    // When T is const, we need some non-cost object we can cast to void* for
    // the placement new. |dummy_| is that object.
    Dummy dummy_;
    T value_;
  };
};

template <typename T>
inline StatusOr<T>::StatusOr(const Status& status) {
  if (status.ok()) {
    status_ = Status::Unknown("Unknown status");
  } else {
    status_ = status;
  }
}

template <typename T>
inline StatusOr<T>::StatusOr(Status&& status) {
  if (status.ok()) {
    status_ = Status::Unknown("Unknown status");
  } else {
    status_ = status;
  }
}

template <typename T>
inline StatusOr<T>::StatusOr(const StatusOr<T>& rhs) : status_(rhs.status_) {
  if (ok()) {
    MakeValue(&dummy_, rhs.value_);
  }
}

template <typename T>
inline StatusOr<T>& StatusOr<T>::operator=(const StatusOr<T>& rhs) {
  if (this != &rhs) {
    status_ = rhs.status_;
    if (rhs.ok()) {
      MakeValue(&dummy_, rhs.value_);
    }
  }
  return *this;
};

template <typename T>
inline StatusOr<T>::StatusOr(StatusOr<T>&& rhs) noexcept
    : status_(std::move(rhs.status_)) {
  if (rhs.ok()) {
    MakeValue(&dummy_, std::move(rhs.value_));
  }
}

template <typename T>
inline StatusOr<T>& StatusOr<T>::operator=(StatusOr<T>&& rhs) noexcept {
  status_ = std::move(rhs.status_);
  if (this != &rhs) {
    MakeValue(&dummy_, std::move(rhs.value_));
  }
  return *this;
}

template <typename T>
inline const T& StatusOr<T>::value() const& {
  LOG_ASSERT(ok()) << status_.ToString();
  return value_;
}

template <typename T>
inline T& StatusOr<T>::value() & {
  LOG_ASSERT(ok()) << status_.ToString();
  return value_;
}

template <typename T>
inline T&& StatusOr<T>::value() && {
  LOG_ASSERT(ok()) << status_.ToString();
  return std::move(value_);
}

}  // namespace iomgr

#endif  // LIBIOMGR_INCLUDE_STATUSOR_H_