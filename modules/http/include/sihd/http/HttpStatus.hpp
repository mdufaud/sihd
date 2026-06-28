#ifndef __SIHD_HTTP_HTTPSTATUS_HPP__
#define __SIHD_HTTP_HTTPSTATUS_HPP__

#include <cstdint>
#include <string_view>

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
constexpr uint32_t TemporaryRedirect = 307;
constexpr uint32_t PermanentRedirect = 308;

// 4xx Client Error
constexpr uint32_t BadRequest = 400;
constexpr uint32_t Unauthorized = 401;
constexpr uint32_t Forbidden = 403;
constexpr uint32_t NotFound = 404;
constexpr uint32_t MethodNotAllowed = 405;
constexpr uint32_t RequestTimeout = 408;
constexpr uint32_t Conflict = 409;
constexpr uint32_t Gone = 410;
constexpr uint32_t TooManyRequests = 429;

// 5xx Server Error
constexpr uint32_t InternalServerError = 500;
constexpr uint32_t NotImplemented = 501;
constexpr uint32_t BadGateway = 502;
constexpr uint32_t ServiceUnavailable = 503;

constexpr std::string_view to_string(uint32_t code)
{
    switch (code)
    {
        case Continue: return "Continue";
        case SwitchingProtocols: return "Switching Protocols";
        case Ok: return "OK";
        case Created: return "Created";
        case Accepted: return "Accepted";
        case NoContent: return "No Content";
        case MovedPermanently: return "Moved Permanently";
        case Found: return "Found";
        case SeeOther: return "See Other";
        case NotModified: return "Not Modified";
        case TemporaryRedirect: return "Temporary Redirect";
        case PermanentRedirect: return "Permanent Redirect";
        case BadRequest: return "Bad Request";
        case Unauthorized: return "Unauthorized";
        case Forbidden: return "Forbidden";
        case NotFound: return "Not Found";
        case MethodNotAllowed: return "Method Not Allowed";
        case RequestTimeout: return "Request Timeout";
        case Conflict: return "Conflict";
        case Gone: return "Gone";
        case TooManyRequests: return "Too Many Requests";
        case InternalServerError: return "Internal Server Error";
        case NotImplemented: return "Not Implemented";
        case BadGateway: return "Bad Gateway";
        case ServiceUnavailable: return "Service Unavailable";
        default: return "Unknown";
    }
}

constexpr bool is_redirect(uint32_t code)
{
    return code == 301 || code == 302 || code == 303 || code == 307 || code == 308;
}

constexpr bool is_post_downgrade(uint32_t code)
{
    return code == 301 || code == 302 || code == 303;
}

constexpr bool is_rate_limit(uint32_t code)
{
    return code == 429 || code == 503;
}

} // namespace HttpStatus

} // namespace sihd::http

#endif
