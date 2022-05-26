#ifndef LIBIOMGR_UTIL_OS_ERROR_H_
#define LIBIOMGR_UTIL_OS_ERROR_H_

#include "iomgr/status.h"

namespace iomgr {

Status MapSystemError(int os_error);

Status MapSocketAcceptError(int os_errno);

Status MapSocketConnectError(int os_errno);

}  // namespace iomgr

#endif  // LIBIOMGR_UTIL_OS_ERROR_H_