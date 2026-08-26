#include <sihd/net/NetInterface.hpp>
#include <sihd/net/ip.hpp>
#include <sihd/sys/platform.hpp>

namespace sihd::net
{

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

} // namespace sihd::net
