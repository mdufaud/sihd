#include <sihd/net/NetInterface.hpp>
#include <sihd/net/ip.hpp>
#include <sihd/util/Defer.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/sys/platform.hpp>

#include <string.h>

#if !defined(__SIHD_WINDOWS__)
# include <ifaddrs.h> // getifaddrs
# include <linux/if_link.h>
# include <linux/if_packet.h>
# include <net/if.h>    // macros
# include <sys/types.h> // getifaddrs
#else
# include <iphlpapi.h>
# include <ws2tcpip.h>
#endif

namespace sihd::net
{

using namespace sihd::util;

NetInterface::NetInterface() = default;

NetInterface::~NetInterface() = default;

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

namespace
{
constexpr uint32_t WIFF_UP = 0x1;
constexpr uint32_t WIFF_BROADCAST = 0x2;
constexpr uint32_t WIFF_LOOPBACK = 0x4;
constexpr uint32_t WIFF_POINTOPOINT = 0x8;
constexpr uint32_t WIFF_RUNNING = 0x10;
constexpr uint32_t WIFF_MULTICAST = 0x20;
constexpr uint32_t WIFF_NOARP = 0x40;
} // namespace

bool NetInterface::up() const { return _flags & WIFF_UP; }
bool NetInterface::broadcast() const { return _flags & WIFF_BROADCAST; }
bool NetInterface::loopback() const { return _flags & WIFF_LOOPBACK; }
bool NetInterface::point2point() const { return _flags & WIFF_POINTOPOINT; }
bool NetInterface::running() const { return _flags & WIFF_RUNNING; }
bool NetInterface::noarp() const { return _flags & WIFF_NOARP; }
bool NetInterface::promisc() const { return false; }
bool NetInterface::notrailers() const { return false; }
bool NetInterface::master() const { return false; }
bool NetInterface::slave() const { return false; }
bool NetInterface::all_multicast() const { return false; }
bool NetInterface::supports_multicast() const { return _flags & WIFF_MULTICAST; }

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
#else
    ULONG buf_size = 15000;
    PIP_ADAPTER_ADDRESSES addresses = nullptr;
    ULONG result;

    do
    {
        addresses = (PIP_ADAPTER_ADDRESSES)malloc(buf_size);
        if (addresses == nullptr)
            return std::nullopt;
        result = GetAdaptersAddresses(AF_UNSPEC, GAA_FLAG_INCLUDE_PREFIX, nullptr, addresses, &buf_size);
        if (result == ERROR_BUFFER_OVERFLOW)
        {
            free(addresses);
            addresses = nullptr;
        }
    } while (result == ERROR_BUFFER_OVERFLOW);

    if (result != NO_ERROR)
    {
        if (addresses != nullptr)
            free(addresses);
        return std::nullopt;
    }

    Defer d([&addresses] { free(addresses); });

    for (PIP_ADAPTER_ADDRESSES adapter = addresses; adapter != nullptr; adapter = adapter->Next)
    {
        std::string iface_name = adapter->AdapterName;
        if (!adapter->FriendlyName)
            continue;

        char friendly[256];
        WideCharToMultiByte(CP_UTF8, 0, adapter->FriendlyName, -1, friendly, sizeof(friendly), nullptr, nullptr);
        iface_name = friendly;

        NetInterface & netif = ret[iface_name];

        if (netif.name().empty())
            netif.set_name(iface_name);

        uint32_t flags = 0;
        if (adapter->OperStatus == IfOperStatusUp)
            flags |= WIFF_UP | WIFF_RUNNING;
        if (adapter->IfType == IF_TYPE_SOFTWARE_LOOPBACK)
            flags |= WIFF_LOOPBACK;
        if (adapter->IfType == IF_TYPE_ETHERNET_CSMACD || adapter->IfType == IF_TYPE_IEEE80211)
            flags |= WIFF_BROADCAST | WIFF_MULTICAST;
        if (adapter->IfType == IF_TYPE_TUNNEL || adapter->IfType == IF_TYPE_PPP)
            flags |= WIFF_POINTOPOINT;
        if (adapter->NoMulticast)
            flags &= ~WIFF_MULTICAST;
        if (adapter->IfType == IF_TYPE_SOFTWARE_LOOPBACK || adapter->IfType == IF_TYPE_TUNNEL)
            flags |= WIFF_NOARP;
        netif.set_flags(flags);

        if (adapter->PhysicalAddressLength == 6)
        {
            char macaddrstr[18] = {0};
            sprintf(macaddrstr, "%02X:%02X:%02X:%02X:%02X:%02X",
                    adapter->PhysicalAddress[0], adapter->PhysicalAddress[1],
                    adapter->PhysicalAddress[2], adapter->PhysicalAddress[3],
                    adapter->PhysicalAddress[4], adapter->PhysicalAddress[5]);
            netif.set_macaddr(macaddrstr);
        }

        for (PIP_ADAPTER_UNICAST_ADDRESS ua = adapter->FirstUnicastAddress; ua != nullptr; ua = ua->Next)
        {
            sockaddr *sa = ua->Address.lpSockaddr;
            if (sa->sa_family == AF_INET)
            {
                IpAddr addr(*sa);
                IpAddr netmask;
                IpAddr extra_addr;

                ULONG mask_val;
                if (ConvertLengthToIpv4Mask(ua->OnLinkPrefixLength, &mask_val) == NO_ERROR)
                {
                    sockaddr_in mask_sa = {};
                    mask_sa.sin_family = AF_INET;
                    mask_sa.sin_addr.s_addr = mask_val;
                    netmask = IpAddr(mask_sa);
                }

                netif.set_ipv4(std::move(addr), std::move(netmask), std::move(extra_addr));
            }
            else if (sa->sa_family == AF_INET6)
            {
                IpAddr addr(*sa);
                IpAddr netmask;
                IpAddr extra_addr;
                netif.set_ipv6(std::move(addr), std::move(netmask), std::move(extra_addr));
            }
        }
    }
#endif
    return ret;
}

} // namespace sihd::net
