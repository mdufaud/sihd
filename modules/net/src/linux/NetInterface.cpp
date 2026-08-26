#include <sihd/net/NetInterface.hpp>
#include <sihd/net/ip.hpp>
#include <sihd/sys/platform.hpp>
#include <sihd/util/Defer.hpp>
#include <sihd/util/Logger.hpp>

#include <ifaddrs.h> // getifaddrs
#include <linux/if_link.h>
#include <linux/if_packet.h>
#include <net/if.h>    // macros
#include <sys/types.h> // getifaddrs

namespace sihd::net
{

using namespace sihd::util;

// Interface is running.
bool NetInterface::up() const
{
    return _flags & IFF_UP;
}

// Valid broadcast address set.
bool NetInterface::broadcast() const
{
    return _flags & IFF_BROADCAST;
}

bool NetInterface::loopback() const
{
    return _flags & IFF_LOOPBACK;
}

// Interface is a point-to-point link.
bool NetInterface::point2point() const
{
    return _flags & IFF_POINTOPOINT;
}

// Resources allocated.
bool NetInterface::running() const
{
    return _flags & IFF_RUNNING;
}

// No arp protocol, L2 destination address not set.
bool NetInterface::noarp() const
{
    return _flags & IFF_NOARP;
}

// Interface is in promiscuous mode.
bool NetInterface::promisc() const
{
    return _flags & IFF_PROMISC;
}

// Avoid use of trailers
bool NetInterface::notrailers() const
{
    return _flags & IFF_NOTRAILERS;
}

// Master of a load balancing bundle.
bool NetInterface::master() const
{
    return _flags & IFF_MASTER;
}

// Slave of a load balancing bundle.
bool NetInterface::slave() const
{
    return _flags & IFF_SLAVE;
}

// Receive all multicast packets.
bool NetInterface::all_multicast() const
{
    return _flags & IFF_ALLMULTI;
}

// Supports multicast
bool NetInterface::supports_multicast() const
{
    return _flags & IFF_MULTICAST;
}

std::optional<std::map<std::string, NetInterface>> NetInterface::get_all_interfaces()
{
    std::map<std::string, NetInterface> ret;

    struct ifaddrs *first;
    struct ifaddrs *iface;
    if (getifaddrs(&iface) < 0)
    {
        return std::nullopt;
    }

    first = iface;
    Defer d([&first] { freeifaddrs(first); });

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
                char macaddrstr[18] = {0};
                int len = 0;
                for (int i = 0; i < 6; i++)
                {
                    len += sprintf(macaddrstr + len, "%02X%s", s->sll_addr[i], i < 5 ? ":" : "");
                }
                netif.set_macaddr(macaddrstr);
            }
        }

        iface = iface->ifa_next;
    }
    return ret;
}

} // namespace sihd::net
