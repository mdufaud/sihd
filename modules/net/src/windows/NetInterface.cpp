#include <sihd/net/NetInterface.hpp>
#include <sihd/net/ip.hpp>
#include <sihd/sys/platform.hpp>
#include <sihd/util/Defer.hpp>
#include <sihd/util/Logger.hpp>

#include <cstdio>

#include <iphlpapi.h>
#include <ws2tcpip.h>

namespace sihd::net
{

using namespace sihd::util;

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

std::optional<std::map<std::string, NetInterface>> NetInterface::get_all_interfaces()
{
    std::map<std::string, NetInterface> ret;

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
    return ret;
}

} // namespace sihd::net
