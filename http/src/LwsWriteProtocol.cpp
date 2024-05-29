#include <libwebsockets.h>

#include <sihd/util/Logger.hpp>

#include <sihd/http/LwsWriteProtocol.hpp>

namespace sihd::http
{

void LwsWriteProtocol::set_txt()
{
    protocol = LWS_WRITE_TEXT;
}

void LwsWriteProtocol::set_bin()
{
    protocol = LWS_WRITE_BINARY;
}

void LwsWriteProtocol::set_continue()
{
    protocol = LWS_WRITE_CONTINUATION;
}

void LwsWriteProtocol::set_http()
{
    protocol = LWS_WRITE_HTTP;
}

void LwsWriteProtocol::set_ping()
{
    protocol = LWS_WRITE_PING;
}

void LwsWriteProtocol::set_pong()
{
    protocol = LWS_WRITE_PONG;
}

void LwsWriteProtocol::set_http_final()
{
    protocol = LWS_WRITE_HTTP_FINAL;
}

void LwsWriteProtocol::set_http_headers()
{
    protocol = LWS_WRITE_HTTP_HEADERS;
}

void LwsWriteProtocol::set_http_headers_continue()
{
    protocol = LWS_WRITE_HTTP_HEADERS_CONTINUATION;
}

} // namespace sihd::http
