#ifndef __SIHD_NET_INETSERVER_HPP__
#define __SIHD_NET_INETSERVER_HPP__

#include <expected>

#include <sihd/net/IpAddr.hpp>
#include <sihd/net/Socket.hpp>
#include <sihd/util/Error.hpp>

namespace sihd::net
{

class INetServer
{
    public:
        virtual ~INetServer() = default;

        virtual std::expected<int, sihd::util::Error> accept_client(IpAddr *client_ip = nullptr,
                                                                    int timeout_ms = Socket::blocking_timeout) = 0;
        virtual bool add_client_read(int socket) = 0;
        virtual bool add_client_write(int socket) = 0;
        virtual bool remove_client_read(int socket) = 0;
        virtual bool remove_client_write(int socket) = 0;
};

} // namespace sihd::net

#endif