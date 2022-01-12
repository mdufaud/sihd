#ifndef __SIHD_HTTP_IWEBSOCKETHANDLER_HPP__
# define __SIHD_HTTP_IWEBSOCKETHANDLER_HPP__

# include <sihd/util/Array.hpp>
# include <string>
# include <string_view>
# include <libwebsockets.h>

namespace sihd::http
{

struct LwsWriteProtocol
{
    enum lws_write_protocol write_protocol;
};

class IWebsocketHandler
{
    public:
        virtual ~IWebsocketHandler() {};
		virtual void on_open(const char *protocol_name) = 0;
		virtual bool on_read(const sihd::util::ArrStr & array) = 0;
		virtual bool on_write(sihd::util::ArrStr & array, LwsWriteProtocol *protocol) = 0;
		virtual void on_close() = 0;
};

}

#endif