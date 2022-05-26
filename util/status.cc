// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#include "iomgr/status.h"

namespace iomgr {

const char* Status::CopyState(const char* state) {
  uint32_t size;
  memcpy(&size, state, sizeof(size));
  char* result = new char[size + 5];
  memcpy(result, state, size + 5);
  return result;
}

Status::Status(Code code, const Slice& msg, const Slice& msg2) {
  const uint32_t len1 = static_cast<uint32_t>(msg.size());
  const uint32_t len2 = static_cast<uint32_t>(msg2.size());
  const uint32_t size = len1 + (len2 ? (2 + len2) : 0);
  char* result = new char[size + 5];
  memcpy(result, &size, sizeof(size));
  result[4] = static_cast<char>(code);
  memcpy(result + 5, msg.data(), len1);
  if (len2) {
    result[5 + len1] = ':';
    result[6 + len1] = ' ';
    memcpy(result + 7 + len1, msg2.data(), len2);
  }
  state_ = result;
}

std::string Status::message() const {
  std::string ret;
  uint32_t length;
  memcpy(&length, state_, sizeof(length));
  ret.append(state_ + 5, length);
  return ret;
}

#define COMBINE(param1, param2) (#param1 param2)
#define CASE(code)              \
  case k##code:                 \
    type = COMBINE(code, ": "); \
    break

std::string Status::ToString() const {
  if (state_ == nullptr) {
    return "OK";
  } else {
    char tmp[30];
    const char* type;
    switch (code()) {
      CASE(Ok);
      CASE(Unknown);
      CASE(InvalidArg);
      CASE(NotFound);
      CASE(NotSupported);
      CASE(Corruption);
      CASE(IOError);
      CASE(TryAgain);
      CASE(Unimplemented);
      CASE(NoPermission);
      CASE(OutOfMemory);
      CASE(OutOfRange);
      CASE(InUse);
      CASE(Timeout);
      CASE(Internal);
      default:
        std::snprintf(tmp, sizeof(tmp),
                      "Unknown code(%d): ", static_cast<int>(code()));
        type = tmp;
        break;
    }
    std::string result(type);
    uint32_t length;
    memcpy(&length, state_, sizeof(length));
    result.append(state_ + 5, length);
    return result;
  }
}

}  // namespace iomgr
