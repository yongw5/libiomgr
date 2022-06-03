#include "util/http_parser.h"

#include <glog/logging.h>

#include <map>

#include "iomgr/http/http_request.h"
#include "iomgr/http/http_response.h"

namespace iomgr {

static const std::map<std::string, HTTPMethod> kMethodMap = {
    {"GET", HTTPMethod::kGet},
    {"POST", HTTPMethod::kPost},
    {"PUT", HTTPMethod::kPut},
    {"DELETE", HTTPMethod::kDelete}};

static const std::map<int, HTTPVersion> kVersionMap = {
    {10, HTTPVersion::kHTTP10},
    {11, HTTPVersion::kHTTP11},
    {20, HTTPVersion::kHTTP20}};

template <typename T>
HTTPParser<T>::HTTPParser(T* req_or_res)
    : req_or_res_(CHECK_NOTNULL(req_or_res)),
      parse_state_(kParseFirstLine),
      cur_line_length_(0),
      cur_line_end_length_(2) {}

template <typename T>
bool HTTPParser<T>::Parse(const Slice& msg, size_t* start_of_body) {
  for (size_t i = 0; i < msg.size(); ++i) {
    bool found_body_start = false;
    if (!AddByte(msg[i], &found_body_start)) {
      return false;
    }
    if (found_body_start && start_of_body != nullptr) {
      *start_of_body = i + 1;
    }
  }
  return true;
}

template <typename T>
bool HTTPParser<T>::AddByte(char byte, bool* found_body_start) {
  switch (parse_state_) {
    case kParseFirstLine:
    case kParseHeaders:
      if (cur_line_length_ >= kMaxHeaderLength) {
        LOG(ERROR) << "HTTP header max length ( " << kMaxHeaderLength
                   << ") exceeded";
        return false;
      }
      cur_line_[cur_line_length_++] = byte;
      if (CheckLine()) {
        return FinishLine(found_body_start);
      }
      break;
    case kParseBody:
      req_or_res_->AppendBody(Slice(&byte, 1));
      break;
    default:
      DCHECK(false) << "Should never reach herea";
      break;
      ;
  }
  return true;
}

template <typename T>
bool HTTPParser<T>::CheckLine() {
  if (cur_line_length_ >= 2 && cur_line_[cur_line_length_ - 2] == '\r' &&
      cur_line_[cur_line_length_ - 1] == '\n') {
    // \r\n terminator
    return true;
  } else if (cur_line_length_ >= 2 && cur_line_[cur_line_length_ - 2] == '\n' &&
             cur_line_[cur_line_length_ - 1] == '\r') {
    // \n\r terminator
  } else if (cur_line_length_ >= 1 && cur_line_[cur_line_length_ - 1] == '\n') {
    // \n terminator
    cur_line_end_length_ = 1;
    return true;
  }

  return false;
}

template <typename T>
bool HTTPParser<T>::FinishLine(bool* found_body_start) {
  switch (parse_state_) {
    case kParseFirstLine:
      if (!HandleFirstLine(Type2Type<T>())) {
        return false;
      }
      parse_state_ = kParseHeaders;
      break;
    case kParseHeaders:
      if (cur_line_length_ == cur_line_end_length_) {
        *found_body_start = true;
        parse_state_ = kParseBody;
        break;
      }
      if (!AddHeader()) {
        return false;
      }
      break;
    case kParseBody:
      DCHECK(false) << "Should never reach here";
      break;
    default:
      DCHECK(false) << "Should never reach here";
      break;
  }
  cur_line_length_ = 0;
  return true;
}

template <typename T>
bool HTTPParser<T>::AddHeader() {
  DCHECK_NE(0, cur_line_length_);

  const char* beg = cur_line_;
  const char* cur = beg;
  const char* end = beg + cur_line_length_;

  if (*cur == ' ' || *cur == '\t') {
    LOG(ERROR) << "Continued header lines not supported yet";
    return false;
  }
  while (cur != end && *cur != ':') {
    ++cur;
  }
  if (cur == end) {
    LOG(ERROR) << "Didn't find ':' in header string";
    return false;
  }
  DCHECK_GE(cur, beg);
  Slice key(beg, cur - beg);
  ++cur;  // skip ':'

  while (cur != end && (*cur == ' ' || *cur == '\t')) {
    ++cur;
  }
  DCHECK_GE(end - cur, cur_line_end_length_);
  Slice value(cur, end - cur - cur_line_end_length_);
  req_or_res_->AddHeader(key, value);
  return true;
}

template <typename T>
template <typename U>
bool HTTPParser<T>::HandleFirstLine(Type2Type<U>) {
  return false;
}

#define HELPER(ch)                       \
  if (cur == end || *cur++ != ch) do {   \
      if (cur == end || *cur++ != ch) {  \
        LOG(ERROR) << "Expected " << ch; \
        return false;                    \
      }                                  \
  } while (0)

template <>
template <>
bool HTTPParser<HTTPRequest>::HandleFirstLine(Type2Type<HTTPRequest>) {
  const char* beg = cur_line_;
  const char* cur = beg;
  const char* end = beg + cur_line_length_;

  while (cur != end && *cur++ != ' ') {
  }
  if (cur == end) {
    LOG(ERROR) << "No method on HTTP request line";
    return false;
  }
  std::string method = std::string(beg, cur - beg - 1);
  if (kMethodMap.find(method) == kMethodMap.end()) {
    LOG(ERROR) << "Unsupported method " << method;
    return false;
  }
  req_or_res_->set_method(kMethodMap.at(method));

  beg = cur;
  while (cur != end && *cur++ != ' ') {
  }
  if (cur == end) {
    LOG(ERROR) << "No URI on HTTP request line";
    return false;
  }
  req_or_res_->set_uri(Slice(beg, cur - beg - 1));

  HELPER('H');
  HELPER('T');
  HELPER('T');
  HELPER('P');
  HELPER('/');
  int version = static_cast<int>(*cur++ - '0') * 10;
  ++cur;
  if (cur == end) {
    LOG(ERROR) << "End of line in HTTP version string";
  }
  version += static_cast<int>(*cur++ - '0');
  if (kVersionMap.find(version) == kVersionMap.end()) {
    LOG(ERROR) << "Expected one of HTTP/1.0, HTTP/1.1, or HTTP/2.0";
    return false;
  }
  req_or_res_->set_version(kVersionMap.at(version));

  return true;
}

template <>
template <>
bool HTTPParser<HTTPResponse>::HandleFirstLine(Type2Type<HTTPResponse>) {
  const char* beg = cur_line_;
  const char* cur = beg;
  const char* end = beg + cur_line_length_;

  HELPER('H');
  HELPER('T');
  HELPER('T');
  HELPER('P');
  HELPER('/');

  int version = static_cast<int>(*cur++ - '0') * 10;
  ++cur;
  if (cur == end) {
    LOG(ERROR) << "End of line in HTTP version string";
  }
  version += static_cast<int>(*cur++ - '0');
  if (kVersionMap.find(version) == kVersionMap.end()) {
    LOG(ERROR) << "Expected one of HTTP/1.0, HTTP/1.1, or HTTP/2.0";
    return false;
  }
  req_or_res_->set_version(kVersionMap.at(version));

  HELPER(' ');
  if (cur == end || *cur < '1' || *cur++ > '9') {
    LOG(ERROR) << "Expected status code";
    return false;
  }
  if (cur == end || *cur < '0' || *cur++ > '9') {
    LOG(ERROR) << "Expected status code";
    return false;
  }
  if (cur == end || *cur < '0' || *cur++ > '9') {
    LOG(ERROR) << "Expected status code";
    return false;
  }

  req_or_res_->set_status_code(static_cast<HTTPStatusCode>(
      (cur[-3] - '0') * 100 + (cur[-2] - '0') * 10 + (cur[-1] - '0')));
  if (cur == end || *cur++ != ' ') {
    LOG(ERROR) << "Expected ' '";
    return false;
  }
  return true;
}

template class HTTPParser<HTTPRequest>;

template class HTTPParser<HTTPResponse>;

}  // namespace iomgr
