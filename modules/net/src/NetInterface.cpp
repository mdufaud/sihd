#include <sihd/net/NetInterface.hpp>
#include <sihd/net/ip.hpp>
#include <sihd/sys/platform.hpp>

namespace sihd::net
{

NetInterface::NetInterface() = default;

NetInterface::~NetInterface() = default;

std::string_view NetInterface::kind_str(Kind kind)
{
    switch (kind)
    {
        case Kind::loopback:
            return "loopback";
        case Kind::ethernet:
            return "ethernet";
        case Kind::wifi:
            return "wifi";
        case Kind::cellular:
            return "cellular";
        case Kind::tunnel:
            return "tunnel";
        case Kind::ppp:
            return "ppp";
        case Kind::bridge:
            return "bridge";
        default:
            return "unknown";
    }
}

const std::string & NetInterface::name() const
{
    return _name;
}

const std::string & NetInterface::mac_addr() const
{
    return _mac_address;
}

uint32_t NetInterface::index() const
{
    return _index;
}

NetInterface::Kind NetInterface::kind() const
{
    return _kind;
}

bool NetInterface::hardware() const
{
    return _hardware;
}

uint32_t NetInterface::mtu() const
{
    return _mtu;
}

uint64_t NetInterface::link_speed_bps() const
{
    return _link_speed_bps;
}

const std::vector<NetInterface::Address> & NetInterface::addresses() const
{
    return _addresses;
}

const NetInterface::Address & NetInterface::reported_address(const std::optional<size_t> & index) const
{
    static const Address none;
    return index ? _addresses[*index] : none;
}

const IpAddr & NetInterface::addr4() const
{
    return this->reported_address(_ipv4_index).addr;
}

const IpAddr & NetInterface::netmask4() const
{
    return this->reported_address(_ipv4_index).netmask;
}

const IpAddr & NetInterface::extra_addr4() const
{
    return this->reported_address(_ipv4_index).extra_addr;
}

const IpAddr & NetInterface::addr6() const
{
    return this->reported_address(_ipv6_index).addr;
}

const IpAddr & NetInterface::netmask6() const
{
    return this->reported_address(_ipv6_index).netmask;
}

const IpAddr & NetInterface::extra_addr6() const
{
    return this->reported_address(_ipv6_index).extra_addr;
}

void NetInterface::set_name(std::string_view name)
{
    _name = name;
}

void NetInterface::set_flags(uint32_t flags)
{
    _flags = flags;
}

void NetInterface::set_macaddr(const std::string & mac_addr)
{
    _mac_address = mac_addr;
}

void NetInterface::set_index(uint32_t index)
{
    _index = index;
}

void NetInterface::set_kind(Kind kind)
{
    _kind = kind;
}

void NetInterface::set_hardware(bool hardware)
{
    _hardware = hardware;
}

void NetInterface::set_mtu(uint32_t mtu)
{
    _mtu = mtu;
}

void NetInterface::set_link_speed_bps(uint64_t link_speed_bps)
{
    _link_speed_bps = link_speed_bps;
}

void NetInterface::add_address(Address && address)
{
    std::optional<size_t> & reported = address.addr.is_ipv6() ? _ipv6_index : _ipv4_index;
    reported = _addresses.size();
    _addresses.push_back(std::move(address));
}

void NetInterface::add_alias_address(Address && address)
{
    _addresses.push_back(std::move(address));
}

} // namespace sihd::net
