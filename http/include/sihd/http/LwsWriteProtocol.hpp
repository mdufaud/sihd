#ifndef __SIHD_HTTP_LWSWRITEPROTOCOL_HPP__
#define __SIHD_HTTP_LWSWRITEPROTOCOL_HPP__

namespace sihd::http
{

struct LwsWriteProtocol
{
        void set_txt();
        void set_bin();
        void set_continue();
        void set_http();
        void set_ping();
        void set_pong();
        void set_http_final();
        void set_http_headers();
        void set_http_headers_continue();

        int protocol;
};

} // namespace sihd::http

#endif