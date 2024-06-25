#ifndef __SIHD_NET_INETSERVER_HPP__
#define __SIHD_NET_INETSERVER_HPP__

#include <sihd/net/IpAddr.hpp>
#include <sihd/util/ABlockingService.hpp>

namespace sihd::net
{

class INetServer: public sihd::util::ABlockingService
{
    public:
        virtual ~INetServer() {};
        virtual int accept_client(IpAddr *client_ip = nullptr) = 0;
        virtual bool add_client_read(int socket) = 0;
        virtual bool add_client_write(int socket) = 0;
        virtual bool remove_client_read(int socket) = 0;
        virtual bool remove_client_write(int socket) = 0;
};

} // namespace sihd::net

#endif