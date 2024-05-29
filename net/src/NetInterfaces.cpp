#include <sihd/util/Logger.hpp>

#include <sihd/net/NetInterfaces.hpp>

#if !defined(__SIHD_WINDOWS__)
# include <ifaddrs.h>   // getifaddrs
# include <net/if.h>    // macros
# include <sys/types.h> // getifaddrs
#endif

namespace sihd::net
{

SIHD_LOGGER;

NetInterfaces::NetInterfaces()
{
    _error = false;
#if !defined(__SIHD_WINDOWS__)
    _addrs_ptr = nullptr;
#endif
    this->load();
}

NetInterfaces::~NetInterfaces()
{
    this->_free();
}

void NetInterfaces::_free()
{
#if !defined(__SIHD_WINDOWS__)
    if (_addrs_ptr != nullptr)
    {
        freeifaddrs(_addrs_ptr);
        _addrs_ptr = nullptr;
        _error = false;
        _ifaces_map.clear();
    }
#endif
}

bool NetInterfaces::error()
{
    return _error;
}

bool NetInterfaces::load()
{
#if !defined(__SIHD_WINDOWS__)
    struct ifaddrs *current;

    this->_free();
    if (getifaddrs(&_addrs_ptr) < 0)
    {
        _error = true;
        return false;
    }
    current = _addrs_ptr;
    while (current)
    {
        NetIFace & iface = _ifaces_map[current->ifa_name];
        iface.add(current);
        current = current->ifa_next;
    }
    return true;
#else
    return false;
#endif
}

std::vector<std::string> NetInterfaces::names()
{
    std::vector<std::string> ret;
    ret.reserve(_ifaces_map.size());
    for (const auto & pair : _ifaces_map)
    {
        ret.push_back(pair.second.name());
    }
    return ret;
}

std::vector<NetIFace *> NetInterfaces::ifaces()
{
    std::vector<NetIFace *> ret;
    ret.reserve(_ifaces_map.size());
    for (auto & pair : _ifaces_map)
    {
        ret.push_back(&pair.second);
    }
    return ret;
}

// NetIFace

#if !defined(__SIHD_WINDOWS__)
void NetIFace::add(struct ifaddrs *addr)
{
    if (_name.empty())
    {
        // first time
        _name = addr->ifa_name;
        _flags = addr->ifa_flags;
    }
    _addrs.push_back(addr);
}

const struct ifaddrs *NetIFace::get_addr(int family) const
{
    for (const struct ifaddrs *addr : _addrs)
    {
        if (addr->ifa_addr->sa_family == family)
            return addr;
    }
    return nullptr;
}

const struct ifaddrs *NetIFace::find(sockaddr_in addr_in) const
{
    struct sockaddr_in *cast;
    for (const struct ifaddrs *addr : _addrs)
    {
        if (addr->ifa_addr->sa_family == AF_INET)
        {
            cast = (struct sockaddr_in *)(addr->ifa_addr);
            if (addr_in.sin_addr.s_addr == cast->sin_addr.s_addr)
                return addr;
        }
    }
    return nullptr;
}

const struct ifaddrs *NetIFace::find(sockaddr_in6 addr_in6) const
{
    struct sockaddr_in6 *cast;
    for (const struct ifaddrs *addr : _addrs)
    {
        if (addr->ifa_addr->sa_family == AF_INET6)
        {
            cast = (struct sockaddr_in6 *)(addr->ifa_addr);
            if (memcmp(addr_in6.sin6_addr.s6_addr, cast->sin6_addr.s6_addr, 16) == 0)
                return addr;
        }
    }
    return nullptr;
}

bool NetIFace::get_netmask(const struct ifaddrs *addr, in_addr *ret) const
{
    if (addr->ifa_addr->sa_family == AF_INET)
    {
        ret->s_addr = (((struct sockaddr_in *)addr->ifa_netmask)->sin_addr.s_addr);
        return true;
    }
    return false;
}

bool NetIFace::get_netmask(const struct ifaddrs *addr, in6_addr *ret) const
{
    if (addr->ifa_addr->sa_family == AF_INET6)
    {
        struct in6_addr *to_copy = &(((struct sockaddr_in6 *)addr->ifa_netmask)->sin6_addr);
        memcpy(ret, to_copy, sizeof(struct in6_addr));
        return true;
    }
    return false;
}

// Interface is running.
bool NetIFace::up() const
{
    return _flags & IFF_UP;
}
// Valid broadcast address set.
bool NetIFace::broadcast() const
{
    return _flags & IFF_BROADCAST;
}
bool NetIFace::loopback() const
{
    return _flags & IFF_LOOPBACK;
}
// Interface is a point-to-point link.
bool NetIFace::point2point() const
{
    return _flags & IFF_POINTOPOINT;
}
// Resources allocated.
bool NetIFace::running() const
{
    return _flags & IFF_RUNNING;
}
// No arp protocol, L2 destination address not set.
bool NetIFace::noarp() const
{
    return _flags & IFF_NOARP;
}
// Interface is in promiscuous mode.
bool NetIFace::promisc() const
{
    return _flags & IFF_PROMISC;
}
// Avoid use of trailers
bool NetIFace::notrailers() const
{
    return _flags & IFF_NOTRAILERS;
}
// Master of a load balancing bundle.
bool NetIFace::master() const
{
    return _flags & IFF_MASTER;
}
// Slave of a load balancing bundle.
bool NetIFace::slave() const
{
    return _flags & IFF_SLAVE;
}
// Receive all multicast packets.
bool NetIFace::all_multicast() const
{
    return _flags & IFF_ALLMULTI;
}
// Supports multicast
bool NetIFace::supports_multicast() const
{
    return _flags & IFF_MULTICAST;
}

#endif

} // namespace sihd::net
