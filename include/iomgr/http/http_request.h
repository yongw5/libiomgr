#ifndef LIBIOMGR_INCLUDE_HTTP_HTTP_REQUEST_H_
#define LIBIOMGR_INCLUDE_HTTP_HTTP_REQUEST_H_

#include <string>
#include <vector>

#include "iomgr/export.h"
#include "iomgr/http/http_types.h"
#include "iomgr/slice.h"

namespace iomgr {

class IOMGR_EXPORT HTTPRequest {
 public:
  HTTPRequest()
      : method_(HTTPMethod::kUnknow),
        version_(HTTPVersion::kHTTP11),
        content_length_(-1) {}

  HTTPMethod method() const { return method_; }
  void set_method(HTTPMethod method) { method_ = method; }

  const std::string& uri() const { return uri_; }
  void set_uri(const Slice& uri) { uri_ = std::string(uri.data(), uri.size()); }

  HTTPVersion version() const { return version_; }
  void set_version(HTTPVersion version) { version_ = version; }

  const std::vector<HTTPHeader>& headers() const { return headers_; }
  void AddHeader(const Slice& key, const Slice& value);

  size_t content_length() const { return content_length_; }

  const std::string& body() const { return body_; }
  void AppendBody(const Slice& body) { body_.append(body.data(), body.size()); }

  std::string ToString() const;

 private:
  // Method of the request (e.g. GET, POST)
  HTTPMethod method_;
  // The path of the resouce to fetch
  std::string uri_;
  // HTTP version to use
  HTTPVersion version_;
  // Headers attached to the request
  std::vector<HTTPHeader> headers_;
  // Content length set
  size_t content_length_;
  // Body
  std::string body_;
};

}  // namespace iomgr

#endif