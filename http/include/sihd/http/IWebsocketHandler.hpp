#ifndef __SIHD_HTTP_IWEBSOCKETHANDLER_HPP__
# define __SIHD_HTTP_IWEBSOCKETHANDLER_HPP__

# include <string>
# include <string_view>

# include <libwebsockets.h>

# include <sihd/util/Array.hpp>

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
		virtual void on_open(std::string_view protocol_name) = 0;
		virtual bool on_read(const sihd::util::ArrChar & array) = 0;
		virtual bool on_write(sihd::util::ArrChar & array, LwsWriteProtocol & protocol) = 0;
		virtual void on_close() = 0;
};

}

#endif