#include <sihd/util/build.hpp>

#if defined(__SIHD_LINUX__)

# include <ifaddrs.h>
# include <sys/types.h>

# include <algorithm>
# include <cstring>
# include <optional>
# include <span>

# include <fmt/format.h>

# include <sihd/net/NetInterface.hpp>
# include <sihd/net/ip.hpp>
# include <sihd/sys/fs.hpp>
# include <sihd/sys/linux/sysfs.hpp>
# include <sihd/sys/platform.hpp>
# include <sihd/util/Defer.hpp>
# include <sihd/util/Result.hpp>
# include <sihd/util/str.hpp>

# include "interfaces.hpp"
# include <linux/if_link.h>
# include <linux/if_packet.h>
# include <net/if.h>
# include <net/if_arp.h>

using namespace sihd::util;
using namespace sihd::sys;

namespace sihd::net
{

namespace
{

using Kind = NetInterface::Kind;

// glibc's net/if_arp.h lacks it, linux/if_arp.h clashes with net/if.h
constexpr uint64_t arphrd_ip6gre = 823;

struct SysfsLink
{
        Kind kind = Kind::unknown;
        bool hardware = false;
        uint32_t mtu = 0;
        uint64_t link_speed_bps = 0;
};

std::string read_devtype(const std::string & sysfs_dir)
{
    auto uevent = linux_::sysfs::read(sysfs_dir + "uevent");
    if (!uevent)
        return "";
    for (const std::string & line : str::split(*uevent, '\n'))
    {
        const auto [key, value] = str::split_pair(line, "=");
        if (key == "DEVTYPE")
            return value;
    }
    return "";
}

std::optional<Kind> kind_from_devtype(std::string_view devtype)
{
    if (devtype == "wlan")
        return Kind::wifi;
    if (devtype == "wwan")
        return Kind::cellular;
    if (devtype == "bridge")
        return Kind::bridge;
    if (devtype == "wireguard" || devtype == "ovpn")
        return Kind::tunnel;
    if (devtype == "ppp")
        return Kind::ppp;
    return std::nullopt;
}

Kind kind_from_arphrd(uint64_t arphrd)
{
    switch (arphrd)
    {
        case ARPHRD_ETHER:
            return Kind::ethernet;
        case ARPHRD_PPP:
            return Kind::ppp;
        case ARPHRD_RAWIP:
            return Kind::cellular;
        case ARPHRD_NONE:
        case ARPHRD_TUNNEL:
        case ARPHRD_TUNNEL6:
        case ARPHRD_SIT:
        case ARPHRD_IPGRE:
        case arphrd_ip6gre:
            return Kind::tunnel;
        default:
            return Kind::unknown;
    }
}

// devtype goes before the arphrd: qmi raw-ip modems are ARPHRD_NONE like tunnels
Kind link_kind(const std::string & sysfs_dir)
{
    if (fs::exists(sysfs_dir + "tun_flags"))
        return Kind::tunnel;
    if (auto kind = kind_from_devtype(read_devtype(sysfs_dir)))
        return *kind;
    if (auto arphrd = linux_::sysfs::read_uint(sysfs_dir + "type"))
        return kind_from_arphrd(*arphrd);
    return Kind::unknown;
}

// sysfs shows the network namespace it was mounted in: a mismatching ifindex is another link
std::optional<SysfsLink> read_sysfs_link(const std::string & name, uint32_t index)
{
    const std::string sysfs_dir = fmt::format("/sys/class/net/{}/", name);
    if (linux_::sysfs::read_uint(sysfs_dir + "ifindex").value_or(0) != index)
        return std::nullopt;
    return SysfsLink {
        .kind = link_kind(sysfs_dir),
        .hardware = fs::exists(sysfs_dir + "device"),
        .mtu = static_cast<uint32_t>(linux_::sysfs::read_uint(sysfs_dir + "mtu").value_or(0)),
        .link_speed_bps = linux_::sysfs::read_uint(sysfs_dir + "speed").value_or(0) * 1'000'000,
    };
}

NetInterface::Address to_address(const struct ifaddrs & iface)
{
    NetInterface::Address address {.addr = IpAddr(*iface.ifa_addr), .netmask = {}, .extra_addr = {}};
    if (iface.ifa_netmask != nullptr)
        address.netmask = IpAddr(*iface.ifa_netmask);
    if ((iface.ifa_flags & IFF_BROADCAST) && iface.ifa_broadaddr != nullptr)
        address.extra_addr = IpAddr(*iface.ifa_broadaddr);
    if ((iface.ifa_flags & IFF_POINTOPOINT) && iface.ifa_dstaddr != nullptr)
        address.extra_addr = IpAddr(*iface.ifa_dstaddr);
    return address;
}

std::string link_layer_address(const struct ifaddrs & iface)
{
    sockaddr_ll link_address {};
    std::memcpy(&link_address, iface.ifa_addr, sizeof(link_address));
    const size_t size = std::min<size_t>(link_address.sll_halen, sizeof(link_address.sll_addr));
    return ip::mac_str(std::span(link_address.sll_addr).first(size));
}

} // namespace

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
    struct ifaddrs *first = nullptr;
    if (getifaddrs(&first) < 0)
        return std::unexpected(Error::from_errno("could not list interfaces"));
    Defer cleanup([first] { freeifaddrs(first); });

    std::map<std::string, NetInterface> ret;
    for (const struct ifaddrs *iface = first; iface != nullptr; iface = iface->ifa_next)
    {
        if (iface->ifa_name == nullptr)
            continue;

        // a link name never holds ':', an "eth0:1" label is a secondary address of eth0
        const std::string label = iface->ifa_name;
        const std::string name = label.substr(0, label.find(':'));
        NetInterface & netif = ret[name];
        if (netif.name().empty())
            netif.set_name(name);
        netif.set_flags(iface->ifa_flags);

        if (iface->ifa_addr == nullptr)
            continue;
        const int family = iface->ifa_addr->sa_family;
        if (family == AF_PACKET)
            netif.set_macaddr(link_layer_address(*iface));
        else if ((family == AF_INET || family == AF_INET6) && label == name)
            netif.add_address(to_address(*iface));
        else if (family == AF_INET || family == AF_INET6)
            netif.add_alias_address(to_address(*iface));
    }

    for (auto & [name, netif] : ret)
    {
        const uint32_t index = interfaces::index_of(name).value_or(0);
        netif.set_index(index);
        if (netif.loopback())
            netif.set_kind(Kind::loopback);

        const std::optional<SysfsLink> link = read_sysfs_link(name, index);
        if (!link)
            continue;
        if (!netif.loopback())
            netif.set_kind(link->kind);
        netif.set_hardware(link->hardware);
        netif.set_mtu(link->mtu);
        netif.set_link_speed_bps(link->link_speed_bps);
    }
    return ret;
}

} // namespace sihd::net

#endif
