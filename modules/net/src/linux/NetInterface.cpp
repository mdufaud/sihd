#include <ifaddrs.h>
#include <sys/types.h>

#include <cstring>

#include <sihd/net/NetInterface.hpp>
#include <sihd/net/ip.hpp>
#include <sihd/sys/platform.hpp>
#include <sihd/util/Defer.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/Result.hpp>

#include <linux/if_link.h>
#include <linux/if_packet.h>
#include <net/if.h>

using namespace sihd::util;

namespace sihd::net
{

bool NetInterface::up() const
{
    return _flags & IFF_UP;
}

bool NetInterface::broadcast() const
{
    return _flags & IFF_BROADCAST;
}

bool NetInterface::loopback() const
{
    return _flags & IFF_LOOPBACK;
}

bool NetInterface::point2point() const
{
    return _flags & IFF_POINTOPOINT;
}

bool NetInterface::running() const
{
    return _flags & IFF_RUNNING;
}

bool NetInterface::noarp() const
{
    return _flags & IFF_NOARP;
}

bool NetInterface::promisc() const
{
    return _flags & IFF_PROMISC;
}

bool NetInterface::notrailers() const
{
    return _flags & IFF_NOTRAILERS;
}

bool NetInterface::master() const
{
    return _flags & IFF_MASTER;
}

bool NetInterface::slave() const
{
    return _flags & IFF_SLAVE;
}

bool NetInterface::all_multicast() const
{
    return _flags & IFF_ALLMULTI;
}

bool NetInterface::supports_multicast() const
{
    return _flags & IFF_MULTICAST;
}

Result<std::map<std::string, NetInterface>> NetInterface::get_all_interfaces()
{
    std::map<std::string, NetInterface> ret;

    struct ifaddrs *first;
    struct ifaddrs *iface;
    if (getifaddrs(&iface) < 0)
        return std::unexpected(Error::from_errno("could not list interfaces"));

    first = iface;
    Defer cleanup([&first] { freeifaddrs(first); });

    while (iface != nullptr)
    {
        if (iface->ifa_name == nullptr)
        {
            iface = iface->ifa_next;
            continue;
        }

        NetInterface & netif = ret[iface->ifa_name];

        if (netif.name().empty())
            netif.set_name(iface->ifa_name);

        netif.set_flags(iface->ifa_flags);

        if (iface->ifa_addr != nullptr)
        {
            if (iface->ifa_addr->sa_family == AF_INET || iface->ifa_addr->sa_family == AF_INET6)
            {
                IpAddr addr(*iface->ifa_addr);
                IpAddr netmask;
                IpAddr extra_addr;

                if (iface->ifa_netmask != nullptr)
                {
                    netmask = IpAddr(*iface->ifa_netmask);
                }

                if (netif.broadcast() && iface->ifa_broadaddr != nullptr)
                {
                    extra_addr = IpAddr(*iface->ifa_broadaddr);
                }

                if (netif.point2point() && iface->ifa_dstaddr != nullptr)
                {
                    extra_addr = IpAddr(*iface->ifa_dstaddr);
                }

                if (iface->ifa_addr->sa_family == AF_INET)
                {
                    netif.set_ipv4(std::move(addr), std::move(netmask), std::move(extra_addr));
                }
                else if (iface->ifa_addr->sa_family == AF_INET6)
                {
                    netif.set_ipv6(std::move(addr), std::move(netmask), std::move(extra_addr));
                }
            }
            else if (iface->ifa_addr->sa_family == AF_PACKET)
            {
                struct sockaddr_ll *s = (struct sockaddr_ll *)iface->ifa_addr;
                if (s->sll_halen >= 6)
                {
                    char macaddrstr[18] = {0};
                    int len = 0;
                    for (int i = 0; i < 6; i++)
                    {
                        len += sprintf(macaddrstr + len, "%02X%s", s->sll_addr[i], i < 5 ? ":" : "");
                    }
                    netif.set_macaddr(macaddrstr);
                }
            }
        }

        iface = iface->ifa_next;
    }
    return ret;
}

} // namespace sihd::net
