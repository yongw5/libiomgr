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

#include "gmock/gmock.h"
#include "gtest/gtest.h"

using ::testing::ContainerEq;
using ::testing::Contains;
using ::testing::ElementsAre;
using ::testing::Pair;

namespace iomgr {

class URIParserTest : public testing::Test {
 protected:
  static void TestSucceeds(const std::string& uri_text,
                           const std::string& scheme,
                           const std::string& authority,
                           const std::string& path,
                           const std::vector<Query>& query_param_pairs,
                           const std::string& fragment) {
    StatusOr<URI> uri = URI::Parse(uri_text);
    ASSERT_TRUE(uri.ok()) << uri.status().ToString();
    EXPECT_EQ(scheme, uri.value().scheme());
    EXPECT_EQ(authority, uri.value().authority());
    EXPECT_EQ(path, uri.value().path());
    EXPECT_THAT(uri.value().query_parameter_pairs(),
                ContainerEq(query_param_pairs));
    EXPECT_EQ(fragment, uri.value().fragment());
  }

  static void TestFails(const std::string& uri_text) {
    StatusOr<URI> uri = URI::Parse(uri_text);
    ASSERT_FALSE(uri.ok());
  }
};

TEST_F(URIParserTest, BasicExamplesAreParsedCorrectly) {
  TestSucceeds("http://www.google.com", "http", "www.google.com", "", {}, "");
  TestSucceeds("dns:///foo", "dns", "", "/foo", {}, "");
  TestSucceeds("http://www.google.com:90", "http", "www.google.com:90", "", {},
               "");
  TestSucceeds("a192.4-df:foo.coom", "a192.4-df", "", "foo.coom", {}, "");
  TestSucceeds("a+b:foo.coom", "a+b", "", "foo.coom", {}, "");
  TestSucceeds("zookeeper://127.0.0.1:2181/foo/bar", "zookeeper",
               "127.0.0.1:2181", "/foo/bar", {}, "");
  TestSucceeds("dns:foo.com#fragment-all-the-things", "dns", "", "foo.com", {},
               "fragment-all-the-things");
  TestSucceeds("http://localhost:8080/whatzit?mi_casa=su_casa", "http",
               "localhost:8080", "/whatzit", {{"mi_casa", "su_casa"}}, "");
  TestSucceeds("http://localhost:8080/whatzit?1=2#buckle/my/shoe", "http",
               "localhost:8080", "/whatzit", {{"1", "2"}}, "buckle/my/shoe");
}

TEST_F(URIParserTest, UncommonValidExamplesAreParsedCorrectly) {
  TestSucceeds("scheme:path//is/ok", "scheme", "", "path//is/ok", {}, "");
  TestSucceeds("http:?legit", "http", "", "", {{"legit", ""}}, "");
  TestSucceeds("unix:#this-is-ok-too", "unix", "", "", {}, "this-is-ok-too");
  TestSucceeds("http:?legit#twice", "http", "", "", {{"legit", ""}}, "twice");
  TestSucceeds("fake:///", "fake", "", "/", {}, "");
  TestSucceeds("http://local%25host:8080/whatz%25it?1%25=2%25#fragment", "http",
               "local%host:8080", "/whatz%it", {{"1%", "2%"}}, "fragment");
}

TEST_F(URIParserTest, VariousKeyValueAndNonKVQueryParamsAreParsedCorrectly) {
  TestSucceeds("http://foo/path?a&b=B&c=&#frag", "http", "foo", "/path",
               {{"a", ""}, {"b", "B"}, {"c", ""}}, "frag");
}

TEST_F(URIParserTest, ParserTreatsFirstEqualSignAsKVDelimiterInQueryString) {
  TestSucceeds(
      "http://localhost:8080/?too=many=equals&are=present=here#fragged", "http",
      "localhost:8080", "/", {{"too", "many=equals"}, {"are", "present=here"}},
      "fragged");
  TestSucceeds("http://auth/path?foo=bar=baz&foobar===", "http", "auth",
               "/path", {{"foo", "bar=baz"}, {"foobar", "=="}}, "");
}

TEST_F(URIParserTest,
       RepeatedQueryParamsAreSupportedInOrderedPairsButDeduplicatedInTheMap) {
  StatusOr<URI> uri = URI::Parse("http://foo/path?a=2&a=1&a=3");
  ASSERT_TRUE(uri.ok()) << uri.status().ToString();
  // The map stores the last found value.
  // Order matters for query parameter pairs
  ASSERT_THAT(uri.value().query_parameter_pairs(),
              ElementsAre(Query{"a", "2"}, Query{"a", "1"}, Query{"a", "3"}));
}

TEST_F(URIParserTest, AWSExternalAccountRegressionTest) {
  TestSucceeds(
      "https://foo.com:5555/v1/"
      "token-exchange?subject_token=eyJhbGciO&subject_token_type=urn:ietf:"
      "params:oauth:token-type:id_token",
      "https", "foo.com:5555", "/v1/token-exchange",
      {{"subject_token", "eyJhbGciO"},
       {"subject_token_type", "urn:ietf:params:oauth:token-type:id_token"}},
      "");
}

TEST_F(URIParserTest, NonKeyValueQueryStringsWork) {
  TestSucceeds("http://www.google.com?yay-i'm-using-queries", "http",
               "www.google.com", "", {{"yay-i'm-using-queries", ""}}, "");
}

TEST_F(URIParserTest, IPV6StringsAreParsedCorrectly) {
  TestSucceeds("ipv6:[2001:db8::1%252]:12345", "ipv6", "",
               "[2001:db8::1%2]:12345", {}, "");
  TestSucceeds("ipv6:[fe80::90%eth1.sky1]:6010", "ipv6", "",
               "[fe80::90%eth1.sky1]:6010", {}, "");
}

TEST_F(URIParserTest,
       PreviouslyReservedCharactersInUnrelatedURIPartsAreIgnored) {
  // The '?' and '/' characters are not reserved delimiter characters in the
  // fragment. See http://go/rfc/3986#section-3.5
  TestSucceeds("http://foo?bar#lol?", "http", "foo", "", {{"bar", ""}}, "lol?");
  TestSucceeds("http://foo?bar#lol?/", "http", "foo", "", {{"bar", ""}},
               "lol?/");
}

TEST_F(URIParserTest, EncodedCharactersInQueryStringAreParsedCorrectly) {
  TestSucceeds("https://www.google.com/?a=1%26b%3D2&c=3", "https",
               "www.google.com", "/", {{"a", "1&b=2"}, {"c", "3"}}, "");
}

TEST_F(URIParserTest, InvalidPercentEncodingsArePassedThrough) {
  TestSucceeds("x:y?%xx", "x", "", "y", {{"%xx", ""}}, "");
  TestSucceeds("http:?dangling-pct-%0", "http", "", "",
               {{"dangling-pct-%0", ""}}, "");
}

TEST_F(URIParserTest, NullCharactersInURIStringAreSupported) {
  // Artificial examples to show that embedded nulls are supported.
  TestSucceeds(std::string("unix-abstract:\0should-be-ok", 27), "unix-abstract",
               "", std::string("\0should-be-ok", 13), {}, "");
}

TEST_F(URIParserTest, EncodedNullsInURIStringAreSupported) {
  TestSucceeds("unix-abstract:%00x", "unix-abstract", "", std::string("\0x", 2),
               {}, "");
}

TEST_F(URIParserTest, InvalidURIsResultInFailureStatuses) {
  TestFails("xyz");
  TestFails("http://foo?[bar]");
  TestFails("http://foo?x[bar]");
  TestFails("http://foo?bar#lol#");
  TestFails("");
  TestFails(":no_scheme");
  TestFails("0invalid_scheme:must_start/with?alpha");
}

TEST(URITest, PercentEncodePath) {
  EXPECT_EQ(URI::PercentEncodePath(
                // These chars are allowed.
                "abcdefghijklmnopqrstuvwxyz"
                "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                "0123456789"
                "/:@-._~!$&'()*+,;="
                // These chars will be escaped.
                "\\?%#[]^"),
            "abcdefghijklmnopqrstuvwxyz"
            "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
            "0123456789"
            "/:@-._~!$&'()*+,;="
            "%5C%3F%25%23%5B%5D%5E");
}

TEST(URITest, Basic) {
  auto uri =
      URI::Create("http", "server.example.com", "/path/to/file.html", {}, "");
  ASSERT_TRUE(uri.ok()) << uri.status().ToString();
  EXPECT_EQ(uri.value().scheme(), "http");
  EXPECT_EQ(uri.value().authority(), "server.example.com");
  EXPECT_EQ(uri.value().path(), "/path/to/file.html");
  EXPECT_THAT(uri.value().query_parameter_pairs(), testing::ElementsAre());
  EXPECT_EQ(uri.value().fragment(), "");
  EXPECT_EQ("http://server.example.com/path/to/file.html",
            uri.value().ToString());
}

TEST(URITest, NoAuthority) {
  auto uri = URI::Create("http", "", "/path/to/file.html", {}, "");
  ASSERT_TRUE(uri.ok()) << uri.status().ToString();
  EXPECT_EQ(uri.value().scheme(), "http");
  EXPECT_EQ(uri.value().authority(), "");
  EXPECT_EQ(uri.value().path(), "/path/to/file.html");
  EXPECT_THAT(uri.value().query_parameter_pairs(), testing::ElementsAre());
  EXPECT_EQ(uri.value().fragment(), "");
  EXPECT_EQ("http:/path/to/file.html", uri.value().ToString());
}

TEST(URITest, NoAuthorityRelativePath) {
  auto uri = URI::Create("http", "", "path/to/file.html", {}, "");
  ASSERT_TRUE(uri.ok()) << uri.status().ToString();
  EXPECT_EQ(uri.value().scheme(), "http");
  EXPECT_EQ(uri.value().authority(), "");
  EXPECT_EQ(uri.value().path(), "path/to/file.html");
  EXPECT_THAT(uri.value().query_parameter_pairs(), testing::ElementsAre());
  EXPECT_EQ(uri.value().fragment(), "");
  EXPECT_EQ("http:path/to/file.html", uri.value().ToString());
}

TEST(URITest, AuthorityRelativePath) {
  auto uri =
      URI::Create("http", "server.example.com", "path/to/file.html", {}, "");
  ASSERT_FALSE(uri.ok());
  EXPECT_TRUE(uri.status().IsInvalidArg());
  EXPECT_EQ(uri.status().message(),
            "if authority is present, path must start with a '/'");
}

TEST(URITest, QueryParams) {
  auto uri = URI::Create("http", "server.example.com", "/path/to/file.html",
                         {{"key", "value"}, {"key2", "value2"}}, "");
  ASSERT_TRUE(uri.ok()) << uri.status().ToString();
  EXPECT_EQ(uri.value().scheme(), "http");
  EXPECT_EQ(uri.value().authority(), "server.example.com");
  EXPECT_EQ(uri.value().path(), "/path/to/file.html");
  EXPECT_THAT(uri.value().query_parameter_pairs(),
              testing::ElementsAre(
                  testing::AllOf(testing::Field(&Query::key, "key"),
                                 testing::Field(&Query::value, "value")),
                  testing::AllOf(testing::Field(&Query::key, "key2"),
                                 testing::Field(&Query::value, "value2"))));
  EXPECT_EQ(uri.value().fragment(), "");
  EXPECT_EQ("http://server.example.com/path/to/file.html?key=value&key2=value2",
            uri.value().ToString());
}

TEST(URITest, DuplicateQueryParams) {
  auto uri = URI::Create(
      "http", "server.example.com", "/path/to/file.html",
      {{"key", "value"}, {"key2", "value2"}, {"key", "other_value"}}, "");
  ASSERT_TRUE(uri.ok()) << uri.status().ToString();
  EXPECT_EQ(uri.value().scheme(), "http");
  EXPECT_EQ(uri.value().authority(), "server.example.com");
  EXPECT_EQ(uri.value().path(), "/path/to/file.html");
  EXPECT_THAT(
      uri.value().query_parameter_pairs(),
      testing::ElementsAre(
          testing::AllOf(testing::Field(&Query::key, "key"),
                         testing::Field(&Query::value, "value")),
          testing::AllOf(testing::Field(&Query::key, "key2"),
                         testing::Field(&Query::value, "value2")),
          testing::AllOf(testing::Field(&Query::key, "key"),
                         testing::Field(&Query::value, "other_value"))));
  EXPECT_EQ(uri.value().fragment(), "");
  EXPECT_EQ(
      "http://server.example.com/path/to/file.html"
      "?key=value&key2=value2&key=other_value",
      uri.value().ToString());
}

TEST(URITest, Fragment) {
  auto uri = URI::Create("http", "server.example.com", "/path/to/file.html", {},
                         "fragment");
  ASSERT_TRUE(uri.ok()) << uri.status().ToString();
  EXPECT_EQ(uri.value().scheme(), "http");
  EXPECT_EQ(uri.value().authority(), "server.example.com");
  EXPECT_EQ(uri.value().path(), "/path/to/file.html");
  EXPECT_THAT(uri.value().query_parameter_pairs(), testing::ElementsAre());
  EXPECT_EQ(uri.value().fragment(), "fragment");
  EXPECT_EQ("http://server.example.com/path/to/file.html#fragment",
            uri.value().ToString());
}

TEST(URITest, QueryParamsAndFragment) {
  auto uri = URI::Create("http", "server.example.com", "/path/to/file.html",
                         {{"key", "value"}, {"key2", "value2"}}, "fragment");
  ASSERT_TRUE(uri.ok()) << uri.status().ToString();
  EXPECT_EQ(uri.value().scheme(), "http");
  EXPECT_EQ(uri.value().authority(), "server.example.com");
  EXPECT_EQ(uri.value().path(), "/path/to/file.html");
  EXPECT_THAT(uri.value().query_parameter_pairs(),
              testing::ElementsAre(
                  testing::AllOf(testing::Field(&Query::key, "key"),
                                 testing::Field(&Query::value, "value")),
                  testing::AllOf(testing::Field(&Query::key, "key2"),
                                 testing::Field(&Query::value, "value2"))));
  EXPECT_EQ(uri.value().fragment(), "fragment");
  EXPECT_EQ(
      "http://server.example.com/path/to/"
      "file.html?key=value&key2=value2#fragment",
      uri.value().ToString());
}

TEST(URITest, ToStringPercentEncoding) {
  auto uri = URI::Create(
      // Scheme allowed chars.
      "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789+-."
      // Scheme escaped chars.
      "%:/?#[]@!$&'()*,;=",
      // Authority allowed chars.
      "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789"
      "-.+~!$&'()*+,;=:[]@"
      // Authority escaped chars.
      "%/?#",
      // Path allowed chars.
      "/abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789"
      "-._~!$&'()*+,;=:@"
      // Path escaped chars.
      "%?#[]",
      {{// Query allowed chars.
        "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789"
        "-._~!$'()*+,;:@/?"
        // Query escaped chars.
        "%=&#[]",
        // Query allowed chars.
        "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789"
        "-._~!$'()*+,;:@/?"
        // Query escaped chars.
        "%=&#[]"}},
      // Fragment allowed chars.
      "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789"
      "-._~!$'()*+,;:@/?=&"
      // Fragment escaped chars.
      "%#[]");
  ASSERT_TRUE(uri.ok()) << uri.status().ToString();
  EXPECT_EQ(uri.value().scheme(),
            "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789+-."
            "%:/?#[]@!$&'()*,;=");
  EXPECT_EQ(uri.value().authority(),
            "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789"
            "-.+~!$&'()*+,;=:[]@"
            "%/?#");
  EXPECT_EQ(uri.value().path(),
            "/abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789"
            "-._~!$&'()*+,;=:@"
            "%?#[]");
  EXPECT_THAT(
      uri.value().query_parameter_pairs(),
      testing::ElementsAre(testing::AllOf(
          testing::Field(
              &Query::key,
              "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789"
              "-._~!$'()*+,;:@/?"
              "%=&#[]"),
          testing::Field(
              &Query::value,
              "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789"
              "-._~!$'()*+,;:@/?"
              "%=&#[]"))));
  EXPECT_EQ(uri.value().fragment(),
            "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789"
            "-._~!$'()*+,;:@/?=&"
            "%#[]");
  EXPECT_EQ(
      // Scheme
      "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789+-."
      "%25%3A%2F%3F%23%5B%5D%40%21%24%26%27%28%29%2A%2C%3B%3D"
      // Authority
      "://abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789"
      "-.+~!$&'()*+,;=:[]@"
      "%25%2F%3F%23"
      // Path
      "/abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789"
      "-._~!$&'()*+,;=:@"
      "%25%3F%23%5B%5D"
      // Query
      "?abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789"
      "-._~!$'()*+,;:@/?"
      "%25%3D%26%23%5B%5D"
      "=abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789"
      "-._~!$'()*+,;:@/?"
      "%25%3D%26%23%5B%5D"
      // Fragment
      "#abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789"
      "-._~!$'()*+,;:@/?=&"
      "%25%23%5B%5D",
      uri.value().ToString());
}

}  // namespace iomgr

int main(int argc, char** argv) {
  testing::InitGoogleTest(&argc, argv);
  testing::InitGoogleMock(&argc, argv);
  return RUN_ALL_TESTS();
}
