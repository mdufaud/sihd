#include <libwebsockets.h>

#include <sihd/http/WriteProtocol.hpp>

namespace sihd::http
{

int write_protocol_to_lws(WriteProtocol protocol)
{
    switch (protocol)
    {
        case WriteProtocol::Text: return LWS_WRITE_TEXT;
        case WriteProtocol::Binary: return LWS_WRITE_BINARY;
        case WriteProtocol::Continuation: return LWS_WRITE_CONTINUATION;
        case WriteProtocol::Http: return LWS_WRITE_HTTP;
        case WriteProtocol::Ping: return LWS_WRITE_PING;
        case WriteProtocol::Pong: return LWS_WRITE_PONG;
        case WriteProtocol::HttpFinal: return LWS_WRITE_HTTP_FINAL;
        case WriteProtocol::HttpHeaders: return LWS_WRITE_HTTP_HEADERS;
        case WriteProtocol::HttpHeadersContinue: return LWS_WRITE_HTTP_HEADERS_CONTINUATION;
    }
    return LWS_WRITE_TEXT;
}

} // namespace sihd::http
