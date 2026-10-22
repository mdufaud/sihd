#include <sihd/net/NetInterface.hpp>
#include <sihd/net/ip.hpp>
#include <sihd/sys/platform.hpp>

namespace sihd::net
{

NetInterface::NetInterface() = default;

NetInterface::~NetInterface() = default;

const std::string & NetInterface::name() const
{
    return _name;
}

const std::string & NetInterface::mac_addr() const
{
    return _mac_address;
}

const IpAddr & NetInterface::addr4() const
{
    return _addr4;
}

const IpAddr & NetInterface::netmask4() const
{
    return _netmask4;
}

const IpAddr & NetInterface::extra_addr4() const
{
    return _extra_addr4;
}

const IpAddr & NetInterface::addr6() const
{
    return _addr6;
}

const IpAddr & NetInterface::netmask6() const
{
    return _netmask6;
}

const IpAddr & NetInterface::extra_addr6() const
{
    return _extra_addr6;
}

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
