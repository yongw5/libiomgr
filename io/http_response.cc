#include "iomgr/http/http_response.h"

#include <array>

namespace iomgr {

static const char* kCRLF = "\r\n";

static std::array<const char*, 600> kStatusCodeWithDesc = [] {
  std::array<const char*, 600> descs;
  descs[100] = "100 Continue";
  descs[101] = "101 Switching Protocols";
  descs[103] = "103 Early Hints";
  descs[200] = "200 OK";
  descs[201] = "201 Created";
  descs[202] = "202 Accepted";
  descs[203] = "203 Non-Authoritative Information";
  descs[204] = "204 No Content";
  descs[205] = "205 Reset Content";
  descs[206] = "206 Partial Content";
  descs[300] = "300 Multiple Choices";
  descs[301] = "301 Moved Permanently";
  descs[302] = "302 Found";
  descs[303] = "303 See Other";
  descs[304] = "304 Not Modified";
  descs[307] = "307 Temporary Redirect";
  descs[308] = "308 Permanent Redirect";
  descs[400] = "400 Bad Request";
  descs[401] = "401 Unauthorized";
  descs[402] = "402 Payment Required";
  descs[403] = "403 Forbidden";
  descs[404] = "404 Not Found";
  descs[405] = "405 Method Not Allowed";
  descs[406] = "406 Not Acceptable";
  descs[407] = "407 Proxy Authentication Required";
  descs[408] = "408 Request Timeout";
  descs[409] = "409 Conflict";
  descs[410] = "410 Gone";
  descs[411] = "411 Length Required";
  descs[412] = "412 Precondition Failed";
  descs[413] = "413 Payload Too Large";
  descs[414] = "414 URI Too Long";
  descs[415] = "415 Unsupported Media Type";
  descs[416] = "416 Range Not Satisfiable";
  descs[417] = "417 Expectation Failed";
  descs[418] = "418 I'm a teapot";
  descs[422] = "422 Unprocessable Entity";
  descs[425] = "425 Too Early";
  descs[426] = "426 Upgrade Required";
  descs[428] = "428 Precondition Required";
  descs[429] = "429 Too Many Requests";
  descs[500] = "500 Internal Server Error";
  descs[501] = "501 Not Implemented";
  descs[502] = "502 Bad Gateway";
  descs[503] = "503 Service Unavailable";
  descs[504] = "504 Gateway Timeout";
  descs[505] = "505 HTTP Version Not Supported";
  return descs;
}();

HTTPResponse::HTTPResponse()
    : status_code_(kUnknow), version_(HTTPVersion::kHTTP11) {}

HTTPResponse::HTTPResponse(HTTPStatusCode status_code, HTTPVersion version)
    : status_code_(status_code), version_(version) {}

void HTTPResponse::AddHeader(const Slice& key, const Slice& value) {
  headers_.push_back({std::string(key.data(), key.size()),
                      std::string(value.data(), value.size())});
}

std::string HTTPResponse::ToString() const {
  std::string ret;
  ret.append(VersionName(version_))
      .append(" ")
      .append(kStatusCodeWithDesc[status_code_])
      .append(kCRLF);

  for (auto& header : headers_) {
    ret.append(header.key).append(": ").append(header.value).append(kCRLF);
  }
  ret.append(kCRLF);
  ret.append(body_);
  return ret;
}

}  // namespace iomgr
