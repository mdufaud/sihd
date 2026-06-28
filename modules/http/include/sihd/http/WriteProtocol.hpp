#ifndef __SIHD_HTTP_WRITEPROTOCOL_HPP__
#define __SIHD_HTTP_WRITEPROTOCOL_HPP__

namespace sihd::http
{

enum class WriteProtocol
{
    Text,
    Binary,
    Continuation,
    Http,
    Ping,
    Pong,
    HttpFinal,
    HttpHeaders,
    HttpHeadersContinue,
};

} // namespace sihd::http

#endif
