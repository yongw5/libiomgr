#ifndef LIBIOMGR_INCLUDE_HTTP_HTTP_TYPES_H_
#define LIBIOMGR_INCLUDE_HTTP_HTTP_TYPES_H_

#include <string>

#include "iomgr/export.h"

namespace iomgr {

enum IOMGR_EXPORT HTTPStatusCode {
  kUnknow = -1,
#define HTTP_STATUS(code, label, meaning) k##label = code
  // Informational responses (100 - 199)
  HTTP_STATUS(100, Continue, "Continue"),
  HTTP_STATUS(101, SwitchingProtocols, "Switching Protocols"),
  HTTP_STATUS(103, EarlyHints, "Eearly Hints"),
  // Successful responses (200 - 299)
  HTTP_STATUS(200, Ok, "OK"),
  HTTP_STATUS(201, Created, "Created"),
  HTTP_STATUS(202, Accepted, "Accepted"),
  HTTP_STATUS(203, NonAuthInfo, "Non-Authoritative Information"),
  HTTP_STATUS(204, NoContent, "No Content"),
  HTTP_STATUS(205, ResetContent, "Reset Content"),
  HTTP_STATUS(206, PartialContent, "Partial Content"),
  // Rediection messages (300 - 399)
  HTTP_STATUS(300, MultiChoices, "Multiple Choices"),
  HTTP_STATUS(301, MovedPermanently, "Moved Permanently"),
  HTTP_STATUS(302, Found, "Found"),
  HTTP_STATUS(303, SeeOther, "See Other"),
  HTTP_STATUS(304, NotModified, "Not Modified"),
  HTTP_STATUS(305, UseProxy, "Use Proxy"),
  HTTP_STATUS(307, TemporaryRedirect, "Temporary Redirect"),
  HTTP_STATUS(308, PermanentRedirect, "Permanent Redirect"),
  // Client error responses (400 - 499)
  HTTP_STATUS(400, BadRequest, "Bad Request"),
  HTTP_STATUS(401, Unauthorized, "Unauthorized"),
  HTTP_STATUS(402, PaymentRequired, "Payment Required"),
  HTTP_STATUS(403, Forbidden, "Forbidden"),
  HTTP_STATUS(404, NotFound, "Not Found"),
  HTTP_STATUS(405, MethodNotAllowed, "Method Not Allowed"),
  HTTP_STATUS(406, NotAcceptable, "Not Acceptable"),
  HTTP_STATUS(407, ProxyAuthRequired, "Proxy Authentication Required"),
  HTTP_STATUS(408, RequestTimeout, "Request Timeout"),
  HTTP_STATUS(409, Conflict, "Conflict"),
  HTTP_STATUS(410, Gone, "Gone"),
  HTTP_STATUS(411, LengthRequired, "Length Required"),
  HTTP_STATUS(412, PreConditionFailed, "Precondition Failed"),
  HTTP_STATUS(413, RequestEntityTooLarge, "Request Entity Too Large"),
  HTTP_STATUS(414, RequestUriTooLong, "Request-URI Too Long"),
  HTTP_STATUS(415, UnsupportedMediaType, "Unsupported Media Type"),
  HTTP_STATUS(416, RequestedRangeNotSatisfiable,
              "Requested Range Not Satisfiable"),
  HTTP_STATUS(417, ExpectationFailed, "Expectation Failed"),
  HTTP_STATUS(418, InvalidXPrivetToken, "Invalid XPrivet Token"),
  HTTP_STATUS(425, TooEarly, "Too Early"),
  HTTP_STATUS(429, TooManyRequests, "Too Many Requests"),
  // Server error responses (500 - 599)
  HTTP_STATUS(500, InternalServerError, "Internal Server Error"),
  HTTP_STATUS(501, NotImplemented, "Not Implemented"),
  HTTP_STATUS(502, BadGateway, "Bad Gateway"),
  HTTP_STATUS(503, ServiceUnavailable, "Service Unavailable"),
  HTTP_STATUS(504, GatewayTimeout, "Gateway Timeout"),
  HTTP_STATUS(505, VersionNotSupported, "HTTP Version Not Supported"),
#undef HTTP_STATUS
};

enum class HTTPMethod : char {
  kUnknow = -1,
  kGet,     // GET
  kPost,    // POST
  kPut,     // PUT
  kDelete,  // DELETE
};

inline const char* MethodName(HTTPMethod method) {
  // caller need to check validation of method
  static const char* method2name[] = {"GET", "POST", "PUT", "DELETE"};
  return method2name[static_cast<size_t>(method)];
}

enum class HTTPVersion : char {
  kUnknow = -1,
  kHTTP10,  // HTTP/1.0
  kHTTP11,  // HTTP/1.1
  kHTTP20,  // HTTP/2.0
};

inline const char* VersionName(HTTPVersion version) {
  static const char* version2name[] = {"HTTP/1.0", "HTTP/1.1", "HTTP/2.0"};
  return version2name[static_cast<size_t>(version)];
}

struct HTTPHeader {
  std::string key;
  std::string value;
};

}  // namespace iomgr

#endif  // LIBIOMGR_INCLUDE_HTTP_HTTP_TYPES_H_