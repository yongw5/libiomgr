#ifndef LIBIOMGR_INCLUDE_CALLBACK_H_
#define LIBIOMGR_INCLUDE_CALLBACK_H_

#include <functional>

#include "iomgr/status.h"
#include "iomgr/statusor.h"

namespace iomgr {

using StatusCallback = std::function<void(Status)>;
using StatusOrIntCallback = std::function<void(StatusOr<int>)>;
using Closure = std::function<void()>;

}  // namespace iomgr

#endif