//
// Copyright 2015 gRPC authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#include "iomgr/http/uri_parser.h"

#include <string.h>

#include <functional>
#include <string>

namespace iomgr {

static int HexDigitToInt(char c) {
  DCHECK(isxdigit(c));
  int x = static_cast<unsigned char>(c);
  if (x > '9') {
    x += 9;
  }
  return x & 0xf;
}

static char IntToHexDigit(int x) { return "0123456789ABCDEF"[x]; }

// Set of ASCII characters (0-127) represented as a bit vector.
class ASCIISet {
 public:
  /// Constructs an empty set.
  ASCIISet() : bitvec_{0, 0} {}

  /// Constructs a set of the characters in `s`.
  ASCIISet(const Slice& s) : bitvec_{0, 0} {
    for (int i = 0; i < s.size(); ++i) {
      Set(s[i]);
    }
  }

  /// Adds a character to the set.
  void Set(char c) {
    auto uc = static_cast<unsigned char>(c);
    bitvec_[(uc & 64) ? 1 : 0] |= static_cast<uint64_t>(1) << (uc & 63);
  }

  /// Returns `true` if `c` is in the set.
  bool Test(char c) const {
    auto uc = static_cast<unsigned char>(c);
    if (uc >= 128) return false;
    return (bitvec_[(uc & 64) ? 1 : 0] >> (uc & 63)) & 1;
  }

 private:
  uint64_t bitvec_[2];
};

static const ASCIISet kURIUnreservedChars{
    "abcdefghijklmnopqrstuvwxyz"
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "0123456789"
    "-_.!~*'()"};

static const ASCIISet kURIPathUnreservedChars{
    "abcdefghijklmnopqrstuvwxyz"
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "0123456789"
    "-_.!~*'():@&=+$,;/"};

static const ASCIISet kURIAuthUnreservedChars{
    "abcdefghijklmnopqrstuvwxyz"
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "0123456789"
    "-_.!~*'():@&=+$,;[]"};

static const ASCIISet kURISchemeUnreseredChars{
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "abcdefghijklmnopqrstuvwxyz"
    "0123456789+-."};

static const ASCIISet kURIQueryKVUnreseredChars{
    "abcdefghijklmnopqrstuvwxyz"
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "0123456789"
    "-_.!~*'():@+$,;/?"};

static const ASCIISet kURIFragmentUnreseredChars{
    "abcdefghijklmnopqrstuvwxyz"
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "0123456789"
    "-_.!~*'():@&=+$,;/?"};

bool IsQueryOrFragmentString(const Slice& str) {
  for (int i = 0; i < str.size(); ++i) {
    if (!kURIFragmentUnreseredChars.Test(str[i]) && str[i] != '%') return false;
  }
  return true;
}

static Status MakeInvalidURIStatus(const Slice& part_name, const Slice& uri,
                                   const Slice& extra) {
  std::string msg = "Could not parse ";
  msg.append(part_name.data(), part_name.size());
  msg += " from uri ";
  msg.append(uri.data(), uri.size());
  msg += ". ";
  msg.append(extra.data(), extra.size());
  return Status::InvalidArg(msg);
}

static std::string PercentEncode(const Slice& str, const ASCIISet& unreserved) {
  std::string ret;
  size_t num_escaped = 0;
  for (int i = 0; i < str.size(); ++i) {
    if (!unreserved.Test(str[i])) ++num_escaped;
  }
  if (num_escaped == 0) {
    ret = std::string(str.data(), str.size());
    return ret;
  }
  ret.clear();
  ret.reserve(str.size() + 2 * num_escaped);
  for (int i = 0; i < str.size(); ++i) {
    if (unreserved.Test(str[i])) {
      ret += str[i];
    } else {
      ret += '%';
      ret += IntToHexDigit(static_cast<unsigned char>(str[i]) / 16);
      ret += IntToHexDigit(static_cast<unsigned char>(str[i]) % 16);
    }
  }
  return ret;
}

// return |slice| if not found
static Slice FindFirstOf(const Slice& slice, const char* t) {
  ASCIISet target(t);
  Slice ret = slice;
  for (size_t i = 0; i < slice.size(); ++i) {
    if (target.Test(slice[i])) {
      ret = Slice(slice.data(), i);
      break;
    }
  }
  return ret;
}

StatusOr<URI> URI::Parse(const Slice& uri_text) {
  Slice remaining = uri_text;
  // parse scheme
  Slice substr = FindFirstOf(remaining, ":");
  if (substr.empty() || substr.size() == remaining.size()) {
    return MakeInvalidURIStatus("scheme", uri_text, "Scheme not found.");
  }

  std::string scheme(substr.data(), substr.size());
  if (scheme.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                               "abcdefghijklmnopqrstuvwxyz"
                               "0123456789+-.") != std::string::npos) {
    return MakeInvalidURIStatus("scheme", uri_text,
                                "Scheme contains invalid characters.");
  }
  if (!isalpha(scheme[0])) {
    return MakeInvalidURIStatus(
        "scheme", uri_text,
        "Scheme must begin with an alpha character [A-Za-z].");
  }

  // parse authority
  remaining.remove_prefix(substr.size() + 1);
  std::string authority;
  if (remaining.starts_with("//")) {
    remaining.remove_prefix(2);
    substr = FindFirstOf(remaining, "/?#");
    authority = PercentDecode(substr);
    remaining.remove_prefix(substr.size());
  }

  // parse path
  std::string path;
  if (!remaining.empty()) {
    substr = FindFirstOf(remaining, "?#");
    path = PercentDecode(substr);
    remaining.remove_prefix(substr.size());
  }

  // parse query
  std::vector<Query> query_param_pairs;
  if (remaining.starts_with("?")) {
    remaining.remove_prefix(1);
    substr = FindFirstOf(remaining, "#");
    if (substr.empty()) {
      return MakeInvalidURIStatus("query", uri_text, "Invalid query string.");
    }
    if (!IsQueryOrFragmentString(substr)) {
      return MakeInvalidURIStatus("query string", uri_text,
                                  "Query string contains invalid characters.");
    }

    remaining.remove_prefix(substr.size());
    while (!substr.empty()) {
      Slice query = FindFirstOf(substr, "&");
      Slice key = FindFirstOf(query, "=");
      substr.remove_prefix(query.size());
      if (substr.starts_with("&")) {
        substr.remove_prefix(1);
      }
      query.remove_prefix(key.size());
      if (query.starts_with("=")) {
        query.remove_prefix(1);
      }
      query_param_pairs.push_back({PercentDecode(key), PercentDecode(query)});
    }
  }

  // parse fragment
  std::string fragment;
  if (remaining.starts_with("#")) {
    remaining.remove_prefix(1);
    if (!IsQueryOrFragmentString(remaining)) {
      return MakeInvalidURIStatus("fragment", uri_text,
                                  "Fragment contains invalid characters.");
    }
    fragment = PercentDecode(remaining);
  }

  return URI(std::move(scheme), std::move(authority), std::move(path),
             std::move(query_param_pairs), std::move(fragment));
}

StatusOr<URI> URI::Create(const Slice& scheme, const Slice& authority,
                          const Slice& path,
                          std::vector<Query> query_parameter_pairs,
                          const Slice& fragment) {
  if (!authority.empty() && !path.empty() && path[0] != '/') {
    return Status::InvalidArg(
        "if authority is present, path must start with a '/'");
  }

  return URI(std::string(scheme.data(), scheme.size()),
             std::string(authority.data(), authority.size()),
             std::string(path.data(), path.size()),
             std::move(query_parameter_pairs),
             std::string(fragment.data(), fragment.size()));
}

std::string URI::PercentEncodePath(const Slice& str) {
  return PercentEncode(str, kURIPathUnreservedChars);
}

std::string URI::PercentDecode(const Slice& src) {
  std::string ret;
  ret.reserve(src.size());
  for (size_t i = 0; i < src.size();) {
    char c = src[i];
    char x, y;
    if (c != '%' || i + 2 >= src.size() || !isxdigit((x = src[i + 1])) ||
        !isxdigit((y = src[i + 2]))) {
      ret += c;
      ++i;
      continue;
    }
    ret += static_cast<char>(HexDigitToInt(x) * 16 + HexDigitToInt(y));
    i += 3;
  }
  return ret;
}

URI::URI(std::string scheme, std::string authority, std::string path,
         std::vector<Query> query_parameter_pairs, std::string fragment)
    : scheme_(std::move(scheme)),
      authority_(std::move(authority)),
      path_(std::move(path)),
      query_parameter_pairs_(std::move(query_parameter_pairs)),
      fragment_(std::move(fragment)) {}

URI::URI(const URI& other)
    : scheme_(other.scheme_),
      authority_(other.authority_),
      path_(other.path_),
      query_parameter_pairs_(other.query_parameter_pairs_),
      fragment_(other.fragment_) {}

URI& URI::operator=(const URI& other) {
  if (this == &other) {
    return *this;
  }
  scheme_ = other.scheme_;
  authority_ = other.authority_;
  path_ = other.path_;
  query_parameter_pairs_ = other.query_parameter_pairs_;
  fragment_ = other.fragment_;
  return *this;
}

std::string URI::ToString() const {
  std::string ret;
  ret.append(PercentEncode(scheme_, kURISchemeUnreseredChars)).push_back(':');
  if (!authority_.empty()) {
    ret.append("//");
    ret.append(PercentEncode(authority_, kURIAuthUnreservedChars));
  }
  if (!path_.empty()) {
    ret.append(PercentEncode(path_, kURIPathUnreservedChars));
  }
  if (!query_parameter_pairs_.empty()) {
    ret.push_back('?');
    for (auto& query : query_parameter_pairs_) {
      ret.append(PercentEncode(query.key, kURIQueryKVUnreseredChars));
      ret.push_back('=');
      ret.append(PercentEncode(query.value, kURIQueryKVUnreseredChars));
      ret.push_back('&');
    }
  }
  if (ret.back() == '&') {
    ret.pop_back();
  }
  if (!fragment_.empty()) {
    ret.push_back('#');
    ret.append(PercentEncode(fragment_, kURIFragmentUnreseredChars));
  }
  return ret;
}

}  // namespace iomgr
