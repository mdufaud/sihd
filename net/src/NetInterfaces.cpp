#include <sihd/net/NetInterfaces.hpp>
#include <sihd/util/Logger.hpp>

namespace sihd::net
{

LOGGER;

NetInterfaces::NetInterfaces()
{
    _error = false;
#if !defined(__SIHD_WINDOWS__)
    _addrs_ptr = nullptr;
#endif
    this->find();
}

NetInterfaces::~NetInterfaces()
{
    this->_free();
}

void    NetInterfaces::_free()
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

bool    NetInterfaces::error()
{
    return _error;
}

bool    NetInterfaces::find()
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

std::vector<std::string>    NetInterfaces::names()
{
    std::vector<std::string> ret;
    ret.reserve(_ifaces_map.size());
    for (const auto & pair: _ifaces_map)
    {
        ret.push_back(pair.second.name());
    }
    return ret;
}

std::vector<NetIFace *>   NetInterfaces::ifaces()
{
    std::vector<NetIFace *> ret;
    ret.reserve(_ifaces_map.size());
    for (auto & pair: _ifaces_map)
    {
        ret.push_back(&pair.second);
    }
    return ret;
}

// NetIFace

#if !defined(__SIHD_WINDOWS__)
void    NetIFace::add(struct ifaddrs *addr)
{
    if (_name.empty())
    {
        // first time
        _name = addr->ifa_name;
        _flags = addr->ifa_flags;
    }
    _addrs.push_back(addr);
}

const struct ifaddrs    *NetIFace::get_addr(int family) const
{
    for (const struct ifaddrs *addr: _addrs)
    {
        if (addr->ifa_addr->sa_family == family)
            return addr;
    }
    return nullptr;
}
#endif

}