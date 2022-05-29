#ifndef LIGIOMGR_INCLUDE_HTTP_CLIENT_
#define LIGIOMGR_INCLUDE_HTTP_CLIENT_

#include <functional>

#include "iomgr/export.h"
#include "iomgr/status.h"
#include "iomgr/time.h"

namespace iomgr {

class HTTPRequest;
class HTTPResponse;
class InetAddress;

class IOMGR_EXPORT HTTPClient {
 public:
  using RequestCb = std::function<void(Status)>;

  static void IssueRequest(const InetAddress& remote,
                           const HTTPRequest& request, RequestCb on_done,
                           HTTPResponse* response);
};

}  // namespace iomgr

#endif  // LIGIOMGR_INCLUDE_HTTP_CLIENT_