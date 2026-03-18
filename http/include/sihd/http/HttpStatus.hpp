#ifndef __SIHD_HTTP_HTTPSTATUS_HPP__
#define __SIHD_HTTP_HTTPSTATUS_HPP__

#include <cstdint>

namespace sihd::http
{

namespace HttpStatus
{
// 1xx Informational
constexpr uint32_t Continue = 100;
constexpr uint32_t SwitchingProtocols = 101;

// 2xx Success
constexpr uint32_t Ok = 200;
constexpr uint32_t Created = 201;
constexpr uint32_t Accepted = 202;
constexpr uint32_t NoContent = 204;

// 3xx Redirection
constexpr uint32_t MovedPermanently = 301;
constexpr uint32_t Found = 302;
constexpr uint32_t SeeOther = 303;
constexpr uint32_t NotModified = 304;

// 4xx Client Error
constexpr uint32_t BadRequest = 400;
constexpr uint32_t Unauthorized = 401;
constexpr uint32_t Forbidden = 403;
constexpr uint32_t NotFound = 404;
constexpr uint32_t MethodNotAllowed = 405;
constexpr uint32_t RequestTimeout = 408;
constexpr uint32_t Conflict = 409;
constexpr uint32_t Gone = 410;

// 5xx Server Error
constexpr uint32_t InternalServerError = 500;
constexpr uint32_t NotImplemented = 501;
constexpr uint32_t BadGateway = 502;
constexpr uint32_t ServiceUnavailable = 503;
} // namespace HttpStatus

} // namespace sihd::http

#endif
