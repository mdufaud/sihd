#ifndef __SIHD_NET_INETRECEIVER_HPP__
#define __SIHD_NET_INETRECEIVER_HPP__

#include <sihd/net/IpAddr.hpp>
#include <sihd/util/IArray.hpp>

namespace sihd::net
{

class INetReceiver
{
    public:
        virtual ~INetReceiver() = default;
        ;

        virtual ssize_t receive(IpAddr & addr, sihd::util::IArray & arr) = 0;
        virtual ssize_t receive(sihd::util::IArray & arr) = 0;
        virtual bool close() = 0;
};

} // namespace sihd::net

#endif