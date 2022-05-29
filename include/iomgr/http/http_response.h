#ifndef LIBIOMGR_INCLUDE_HTTP_RESPONSE_H_
#define LIBIOMGR_INCLUDE_HTTP_RESPONSE_H_

#include <string>
#include <vector>

#include "iomgr/export.h"
#include "iomgr/http/http_types.h"
#include "iomgr/slice.h"

namespace iomgr {

class IOMGR_EXPORT HTTPResponse {
 public:
  HTTPResponse();

  static HTTPResponse BadRequest() {
    return HTTPResponse(kBadRequest, HTTPVersion::kHTTP11);
  }

  static HTTPResponse Ok() { return HTTPResponse(kOk, HTTPVersion::kHTTP11); }

  HTTPStatusCode status_code() const { return status_code_; }
  void set_status_code(HTTPStatusCode status_code) {
    status_code_ = status_code;
  }
  HTTPVersion version() const { return version_; }
  void set_version(HTTPVersion version) { version_ = version; }

  const std::vector<HTTPHeader>& headers() const { return headers_; }
  void AddHeader(const Slice& key, const Slice& value);

  const std::string& body() const { return body_; }
  void AppendBody(const Slice& body) { body_.append(body.data(), body.size()); }

  std::string ToString() const;

 private:
  HTTPResponse(HTTPStatusCode status_code, HTTPVersion version);

  HTTPStatusCode status_code_;
  HTTPVersion version_;
  std::vector<HTTPHeader> headers_;
  std::string body_;
};

}  // namespace iomgr

#endif