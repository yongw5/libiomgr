#ifndef LIBIOMGR_UTIL_HTTP_PARSER_H_
#define LIBIOMGR_UTIL_HTTP_PARSER_H_

#include "iomgr/export.h"
#include "iomgr/slice.h"

namespace iomgr {

const int kMaxHeaderLength = 4096;

template <typename T>
class IOMGR_EXPORT HTTPParser {
 public:
  explicit HTTPParser(T* req_or_res);
  bool Parse(const Slice& msg, size_t* start_of_body);
  bool RecievedAllHeaders() const { return parse_state_ == kParseBody; }

 private:
  enum { kParseFirstLine, kParseHeaders, kParseBody };

  template <typename U>
  struct Type2Type {
    using Type = U;
  };

  bool AddByte(char byte, bool* found_body_start);
  bool CheckLine();
  bool FinishLine(bool* found_body_start);
  bool AddHeader();

  template <typename U>
  bool HandleFirstLine(Type2Type<U>);

  T* req_or_res_;
  int parse_state_;
  size_t cur_line_length_;
  size_t cur_line_end_length_;
  char cur_line_[kMaxHeaderLength];
};

}  // namespace iomgr

#endif  // LIBIOMGR_UTIL_HTTP_PARSER_H_