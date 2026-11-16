#ifndef __SIHD_HTTP_SRC_WRITEPROTOCOL_HPP__
#define __SIHD_HTTP_SRC_WRITEPROTOCOL_HPP__

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

int write_protocol_to_lws(WriteProtocol protocol);

} // namespace sihd::http

#endif
