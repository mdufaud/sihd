#ifndef __SIHD_NET_INETSERVERHANDLER_HPP__
#define __SIHD_NET_INETSERVERHANDLER_HPP__

#include <time.h>

#include <sihd/net/INetServer.hpp>

namespace sihd::net
{

class INetServerHandler
{
    public:
        virtual ~INetServerHandler() = default;
        ;
        virtual void handle_no_activity(INetServer *server, time_t nano) = 0;
        virtual void handle_activity(INetServer *server, time_t nano) = 0;
        virtual void handle_new_client(INetServer *server) = 0;
        virtual void handle_client_read(INetServer *server, int socket) = 0;
        virtual void handle_client_write(INetServer *server, int socket) = 0;
        virtual void handle_after_activity(INetServer *server) = 0;
};

} // namespace sihd::net

#endif