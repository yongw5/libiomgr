#include "iomgr/http/http_request.h"

#include "glog/logging.h"

namespace iomgr {

static const char* kCRLF = "\r\n";
static const Slice kContentLength("content-length");

void HTTPRequest::AddHeader(const Slice& key, const Slice& value) {
  if (content_length_ == -1) {
    size_t i = 0;
    for (; i < kContentLength.size() && i < key.size(); ++i) {
      if (std::tolower(key[i] != kContentLength[i])) {
        break;
      }
    }
    if (i == kContentLength.size()) {
      content_length_ = std::atoi(value.data());
    }
  }
  headers_.push_back({std::string(key.data(), key.size()),
                      std::string(value.data(), value.size())});
}

std::string HTTPRequest::ToString() const {
  std::string ret;
  ret.append(MethodName(method_))
      .append(" ")
      .append(uri_)
      .append(" ")
      .append(VersionName(version_))
      .append(kCRLF);

  for (auto& header : headers_) {
    ret.append(header.key).append(": ").append(header.value).append(kCRLF);
  }
  if (content_length_ == -1) {
    ret.append("content-length")
        .append(": ")
        .append(std::to_string(body_.size()))
        .append(kCRLF);
  }
  ret.append(kCRLF);
  ret.append(body_);
  return ret;
}

}  // namespace iomgr
