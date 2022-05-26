// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.
//
// A Status encapsulates the result of an operation.  It may indicate success,
// or it may indicate an error with an associated error message.
//
// Multiple threads can invoke const methods on a Status without
// external synchronization, but if any of the threads may call a
// non-const method, all threads accessing the same Status must use
// external synchronization.

#ifndef LIBIOMGR_INCLUDE_STATUS_H_
#define LIBIOMGR_INCLUDE_STATUS_H_

#include <algorithm>
#include <string>

#include "iomgr/export.h"
#include "iomgr/slice.h"

namespace iomgr {

class IOMGR_EXPORT Status {
 public:
  // Create a success status.
  Status() noexcept : state_(nullptr) {}
  ~Status() { delete[] state_; }

  Status(const Status& rhs);
  Status& operator=(const Status& rhs);

  Status(Status&& rhs) noexcept : state_(rhs.state_) { rhs.state_ = nullptr; }
  Status& operator=(Status&& rhs) noexcept;

  // Return a success status.
  static Status OK() { return Status(); }
  static Status Unknown(const Slice& msg, const Slice& msg2 = Slice()) {
    return Status(kUnknown, msg, msg2);
  }
  static Status InvalidArg(const Slice& msg, const Slice& msg2 = Slice()) {
    return Status(kInvalidArg, msg, msg2);
  }
  static Status NotFound(const Slice& msg, const Slice& msg2 = Slice()) {
    return Status(kNotFound, msg, msg2);
  }
  static Status NotSupported(const Slice& msg, const Slice& msg2 = Slice()) {
    return Status(kNotSupported, msg, msg2);
  }
  static Status Corruption(const Slice& msg, const Slice& msg2 = Slice()) {
    return Status(kCorruption, msg, msg2);
  }
  static Status IOError(const Slice& msg, const Slice& msg2 = Slice()) {
    return Status(kIOError, msg, msg2);
  }
  static Status TryAgain(const Slice& msg, const Slice& msg2 = Slice()) {
    return Status(kTryAgain, msg, msg2);
  }
  static Status Unimplemented(const Slice& msg, const Slice& msg2 = Slice()) {
    return Status(kUnimplemented, msg, msg2);
  }
  static Status NoPermission(const Slice& msg, const Slice& msg2 = Slice()) {
    return Status(kNoPermission, msg, msg2);
  }
  static Status OutOfMemory(const Slice& msg, const Slice& msg2 = Slice()) {
    return Status(kOutOfMemory, msg, msg2);
  }
  static Status OutOfRange(const Slice& msg, const Slice& msg2 = Slice()) {
    return Status(kOutOfRange, msg, msg2);
  }
  static Status InUse(const Slice& msg, const Slice& msg2 = Slice()) {
    return Status(kInUse, msg, msg2);
  }
  static Status Timeout(const Slice& msg, const Slice& msg2 = Slice()) {
    return Status(kTimeout, msg, msg2);
  }
  static Status Internal(const Slice& msg, const Slice& msg2 = Slice()) {
    return Status(kInternal, msg, msg2);
  }

  bool ok() const { return (state_ == nullptr); }
  bool IsUnknown() const { return code() == kUnknown; }
  bool IsInvalidArg() const { return code() == kInvalidArg; }
  bool IsNotFound() const { return code() == kNotFound; }
  bool IsNotSupported() const { return code() == kNotSupported; }
  bool IsCorruption() const { return code() == kCorruption; }
  bool IsIOError() const { return code() == kIOError; }
  bool IsTryAgain() const { return code() == kTryAgain; }
  bool IsUnimplemented() const { return code() == kUnimplemented; }
  bool IsNoPermission() const { return code() == kNoPermission; }
  bool IsOutOfMemory() const { return code() == kOutOfMemory; }
  bool IsOutOfRange() const { return code() == kOutOfRange; }
  bool IsInUse() const { return code() == kInUse; }
  bool IsTimeout() const { return code() == kTimeout; }
  bool IsInternal() const { return code() == kInternal; }

  // Return a string representation of this status suitable for printing.
  // Returns the string "OK" for success.
  std::string ToString() const;
  
  // return a string
  std::string message() const;

 private:
  enum Code : int {
    kOk = 0,
    kUnknown,
    kInvalidArg,
    kNotFound,
    kNotSupported,
    kCorruption,
    kIOError,
    kTryAgain,
    kUnimplemented,
    kNoPermission,
    kOutOfMemory,
    kOutOfRange,
    kInUse,
    kTimeout,
    kInternal,
  };

  Code code() const {
    return (state_ == nullptr) ? kOk : static_cast<Code>(state_[4]);
  }

  Status(Code code, const Slice& msg, const Slice& msg2);
  static const char* CopyState(const char* s);

  // OK status has a null state_.  Otherwise, state_ is a new[] array
  // of the following form:
  //    state_[0..3] == length of message
  //    state_[4]    == code
  //    state_[5..]  == message
  const char* state_;
};

inline Status::Status(const Status& rhs) {
  state_ = (rhs.state_ == nullptr) ? nullptr : CopyState(rhs.state_);
}
inline Status& Status::operator=(const Status& rhs) {
  // The following condition catches both aliasing (when this == &rhs),
  // and the common case where both rhs and *this are ok.
  if (state_ != rhs.state_) {
    delete[] state_;
    state_ = (rhs.state_ == nullptr) ? nullptr : CopyState(rhs.state_);
  }
  return *this;
}
inline Status& Status::operator=(Status&& rhs) noexcept {
  std::swap(state_, rhs.state_);
  return *this;
}

}  // namespace iomgr

#endif  // LIBIOMGR_INCLUDE_STATUS_H_