#include "util/os_error.h"

namespace iomgr {

Status MapSystemError(int os_errno) {
  switch (os_errno) {
    case 0:
      return Status();
    case EPERM:
      return Status::NoPermission(strerror(os_errno));
    case ENOENT:
    case EIO:
    case EBADFD:
    case EADDRNOTAVAIL:
    case ENETDOWN:
    case ENETUNREACH:
    case ENETRESET:
    case ECONNABORTED:
    case ECONNRESET:
    case ENOBUFS:
    case ECONNREFUSED:
    case EISCONN:
    case ENOTCONN:
    case ESHUTDOWN:
    case EHOSTDOWN:
    case EHOSTUNREACH:
      return Status::IOError(strerror(os_errno));
    case E2BIG:
    case EINVAL:
      return Status::InvalidArg(strerror(os_errno));
    case EAGAIN:
#if EWOULDBLOCK != EAGAIN
    case EWOULDBLOCK:
#endif
      return Status::TryAgain(strerror(os_errno));
    case ENOPROTOOPT:
    case EPFNOSUPPORT:
    case EAFNOSUPPORT:
      return Status::Unimplemented(strerror(os_errno));
    case EADDRINUSE:
      return Status::InUse(strerror(os_errno));
    case ETIMEDOUT:
      return Status::Timeout(strerror(os_errno));
    default:
      return Status::Unknown(strerror(os_errno));
  }
}

Status MapSocketAcceptError(int os_errno) {
  switch (os_errno) {
    case ECONNABORTED:
      return Status::TryAgain("IO pending", strerror(os_errno));
    default:
      return MapSystemError(os_errno);
  }
}

Status MapSocketConnectError(int os_errno) {
  switch (os_errno) {
    case EINPROGRESS:
      return Status::TryAgain("IO pending", strerror(os_errno));
    case EACCES:
      return Status::NoPermission("Network acess denied", strerror(os_errno));
    case ETIMEDOUT:
      return Status::Timeout("Connection timeout", strerror(os_errno));
    default:
      return MapSystemError(os_errno);
  }
}

}  // namespace iomgr
