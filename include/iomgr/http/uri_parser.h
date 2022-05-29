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

#ifndef LIBIOMGR_INCLUDE_HTTP_URI_PARSER_H_
#define LIBIOMGR_INCLUDE_HTTP_URI_PARSER_H_

#include "iomgr/export.h"
#include "iomgr/slice.h"
#include "iomgr/statusor.h"

namespace iomgr {

struct IOMGR_EXPORT Query {
  std::string key;
  std::string value;

  bool operator==(const Query& other) const {
    return key == other.key && value == other.value;
  }
};

class IOMGR_EXPORT URI {
 public:
  // Creates a URI by parsing an rfc3986 URI string. Returns an
  // InvalidArgumentError on failure.
  static StatusOr<URI> Parse(const Slice& uri_text);
  // Creates a URI from components. Returns an InvalidArgumentError on failure.
  static StatusOr<URI> Create(const Slice& scheme, const Slice& authority,
                              const Slice& path,
                              std::vector<Query> query_parameter_pairs,
                              const Slice& fragment);

  URI() = default;

  // Copy construction and assignment
  URI(const URI& other);
  URI& operator=(const URI& other);
  // Move construction and assignment
  URI(URI&&) = default;
  URI& operator=(URI&&) = default;

  static std::string PercentEncodePath(const Slice& str);

  static std::string PercentDecode(const Slice& str);

  const std::string& scheme() const { return scheme_; }
  const std::string& authority() const { return authority_; }
  const std::string& path() const { return path_; }

  // A vector of key:value query parameter pairs, kept in order of appearance
  // within the URI search string. Repeated keys are represented as separate
  // key:value elements.
  const std::vector<Query>& query_parameter_pairs() const {
    return query_parameter_pairs_;
  }
  const std::string& fragment() const { return fragment_; }

  std::string ToString() const;

 private:
  URI(std::string scheme, std::string authority, std::string path,
      std::vector<Query> query_parameter_pairs, std::string fragment);

  std::string scheme_;
  std::string authority_;
  std::string path_;
  std::vector<Query> query_parameter_pairs_;
  std::string fragment_;
};

}  // namespace iomgr

#endif  // LIBIOMGR_INCLUDE_HTTP_URI_PARSER_H_