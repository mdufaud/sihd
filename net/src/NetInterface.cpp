#include <sihd/net/NetInterface.hpp>
#include <sihd/net/ip.hpp>
#include <sihd/util/Defer.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/platform.hpp>

#include <string.h>

#if !defined(__SIHD_WINDOWS__)
# include <ifaddrs.h> // getifaddrs
# include <linux/if_link.h>
# include <linux/if_packet.h>
# include <net/if.h>    // macros
# include <sys/types.h> // getifaddrs
#endif

namespace sihd::net
{

using namespace sihd::util;

NetInterface::NetInterface() {}

NetInterface::~NetInterface() {}

void NetInterface::set_name(std::string_view name)
{
    _name = name;
}

void NetInterface::set_flags(uint32_t flags)
{
    _flags = flags;
}

void NetInterface::set_ipv4(IpAddr && addr, IpAddr && netmask, IpAddr && extra)
{
    _addr4 = std::move(addr);
    _netmask4 = std::move(netmask);
    _extra_addr4 = std::move(extra);
}

void NetInterface::set_ipv6(IpAddr && addr, IpAddr && netmask, IpAddr && extra)
{
    _addr6 = std::move(addr);
    _netmask6 = std::move(netmask);
    _extra_addr6 = std::move(extra);
}

void NetInterface::set_macaddr(std::string_view mac_addr)
{
    _mac_address = std::string(mac_addr);
}

#if !defined(__SIHD_WINDOWS__)

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

#else

// Interface is running.
bool NetInterface::up() const
{
    return false;
}

// Valid broadcast address set.
bool NetInterface::broadcast() const
{
    return false;
}

bool NetInterface::loopback() const
{
    return false;
}

// Interface is a point-to-point link.
bool NetInterface::point2point() const
{
    return false;
}

// Resources allocated.
bool NetInterface::running() const
{
    return false;
}

// No arp protocol, L2 destination address not set.
bool NetInterface::noarp() const
{
    return false;
}

// Interface is in promiscuous mode.
bool NetInterface::promisc() const
{
    return false;
}

// Avoid use of trailers
bool NetInterface::notrailers() const
{
    return false;
}

// Master of a load balancing bundle.
bool NetInterface::master() const
{
    return false;
}

// Slave of a load balancing bundle.
bool NetInterface::slave() const
{
    return false;
}

// Receive all multicast packets.
bool NetInterface::all_multicast() const
{
    return false;
}

// Supports multicast
bool NetInterface::supports_multicast() const
{
    return false;
}

#endif

std::optional<std::map<std::string, NetInterface>> NetInterface::get_all_interfaces()
{
    std::map<std::string, NetInterface> ret;

#if !defined(__SIHD_WINDOWS__)
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
            continue;

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
#else
#endif
    return ret;
}

} // namespace sihd::net
