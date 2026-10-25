#include <winsock2.h>

#include <algorithm>
#include <span>

#include <sihd/net/NetInterface.hpp>
#include <sihd/net/ip.hpp>
#include <sihd/sys/platform.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/Result.hpp>

#include "adapters.hpp"

using namespace sihd::util;

namespace sihd::net
{

namespace
{
constexpr uint32_t WIFF_UP = 0x1;
constexpr uint32_t WIFF_BROADCAST = 0x2;
constexpr uint32_t WIFF_LOOPBACK = 0x4;
constexpr uint32_t WIFF_POINTOPOINT = 0x8;
constexpr uint32_t WIFF_RUNNING = 0x10;
constexpr uint32_t WIFF_MULTICAST = 0x20;
constexpr uint32_t WIFF_NOARP = 0x40;

constexpr ULONG64 unknown_link_speed = ~ULONG64 {0};

using Kind = NetInterface::Kind;

// 53 (prop virtual) is what wintun, wireguard-nt, tap-windows6 and ovpn-dco declare
Kind kind_from_iftype(IFTYPE iftype)
{
    switch (iftype)
    {
        case IF_TYPE_SOFTWARE_LOOPBACK:
            return Kind::loopback;
        case IF_TYPE_ETHERNET_CSMACD:
            return Kind::ethernet;
        case IF_TYPE_IEEE80211:
            return Kind::wifi;
        case IF_TYPE_WWANPP:
        case IF_TYPE_WWANPP2:
            return Kind::cellular;
        case IF_TYPE_PPP:
            return Kind::ppp;
        case IF_TYPE_TUNNEL:
        case IF_TYPE_PROP_VIRTUAL:
            return Kind::tunnel;
        default:
            return Kind::unknown;
    }
}

sockaddr_in6 ipv6_netmask(uint8_t prefix_length)
{
    sockaddr_in6 mask {};
    mask.sin6_family = AF_INET6;
    for (uint8_t bit = 0; bit < prefix_length && bit < 128; ++bit)
        mask.sin6_addr.s6_addr[bit / 8] |= static_cast<uint8_t>(0x80 >> (bit % 8));
    return mask;
}

} // namespace

bool NetInterface::up() const
{
    return _flags & WIFF_UP;
}

bool NetInterface::broadcast() const
{
    return _flags & WIFF_BROADCAST;
}

bool NetInterface::loopback() const
{
    return _flags & WIFF_LOOPBACK;
}

bool NetInterface::point2point() const
{
    return _flags & WIFF_POINTOPOINT;
}

bool NetInterface::running() const
{
    return _flags & WIFF_RUNNING;
}

bool NetInterface::noarp() const
{
    return _flags & WIFF_NOARP;
}

bool NetInterface::promisc() const
{
    return false;
}

bool NetInterface::notrailers() const
{
    return false;
}

bool NetInterface::master() const
{
    return false;
}

bool NetInterface::slave() const
{
    return false;
}

bool NetInterface::all_multicast() const
{
    return false;
}

bool NetInterface::supports_multicast() const
{
    return _flags & WIFF_MULTICAST;
}

Result<std::map<std::string, NetInterface>> NetInterface::get_all_interfaces()
{
    std::map<std::string, NetInterface> ret;

    auto adapter_list = adapters::list(GAA_FLAG_INCLUDE_PREFIX);
    SIHD_UNEXPECTED_RETURN(adapter_list);

    for (const IP_ADAPTER_ADDRESSES *adapter : adapters::named(*adapter_list))
    {
        const std::string iface_name = adapters::friendly_name(*adapter);
        NetInterface & netif = ret[iface_name];

        if (netif.name().empty())
            netif.set_name(iface_name);

        netif.set_index(adapter->IfIndex != 0 ? adapter->IfIndex : adapter->Ipv6IfIndex);
        netif.set_kind(kind_from_iftype(adapter->IfType));
        netif.set_mtu(adapter->Mtu);
        netif.set_link_speed_bps(adapter->TransmitLinkSpeed != unknown_link_speed ? adapter->TransmitLinkSpeed : 0);

        MIB_IF_ROW2 row {};
        row.InterfaceLuid = adapter->Luid;
        if (GetIfEntry2(&row) == NO_ERROR)
            netif.set_hardware(row.InterfaceAndOperStatusFlags.HardwareInterface);

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

        netif.set_macaddr(ip::mac_str(std::span(adapter->PhysicalAddress).first(std::min<size_t>(adapter->PhysicalAddressLength, sizeof(adapter->PhysicalAddress)))));

        for (PIP_ADAPTER_UNICAST_ADDRESS ua = adapter->FirstUnicastAddress; ua != nullptr; ua = ua->Next)
        {
            sockaddr *sa = ua->Address.lpSockaddr;
            if (sa->sa_family == AF_INET)
            {
                IpAddr addr(*sa);
                IpAddr netmask;

                ULONG mask_val;
                if (ConvertLengthToIpv4Mask(ua->OnLinkPrefixLength, &mask_val) == NO_ERROR)
                {
                    sockaddr_in mask_sa = {};
                    mask_sa.sin_family = AF_INET;
                    mask_sa.sin_addr.s_addr = mask_val;
                    netmask = IpAddr(mask_sa);
                }

                netif.add_address({.addr = std::move(addr), .netmask = std::move(netmask), .extra_addr = {}});
            }
            else if (sa->sa_family == AF_INET6)
            {
                netif.add_address({.addr = IpAddr(*sa), .netmask = IpAddr(ipv6_netmask(ua->OnLinkPrefixLength)), .extra_addr = {}});
            }
        }
    }
    return ret;
}

} // namespace sihd::net
