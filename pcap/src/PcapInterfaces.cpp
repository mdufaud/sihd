#include <sihd/util/Logger.hpp>
#include <sihd/util/str.hpp>

#include <sihd/pcap/PcapInterfaces.hpp>

namespace sihd::pcap
{

SIHD_NEW_LOGGER("sihd::pcap");

PcapInterfaces::PcapInterfaces(): _code(0), _interfaces_ptr(nullptr)
{
    utils::init();
    this->find();
}

PcapInterfaces::~PcapInterfaces()
{
    this->_free();
}

void PcapInterfaces::_free()
{
    if (_interfaces_ptr != nullptr)
    {
        pcap_freealldevs(_interfaces_ptr);
        _interfaces_ptr = nullptr;
    }
}

struct pcap_addr *PcapInterfaces::pcap_addr(std::string_view name)
{
    pcap_if_t *tmp = _interfaces_ptr;
    struct pcap_addr *addr = nullptr;
    while (tmp)
    {
        if (name == tmp->name)
        {
            addr = tmp->addresses;
            break;
        }
        tmp = tmp->next;
    }
    return addr;
}

std::vector<PcapIFace> PcapInterfaces::ifaces()
{
    std::vector<PcapIFace> ret;
    pcap_if_t *tmp = _interfaces_ptr;
    int n = 0;
    while (tmp)
    {
        ++n;
        tmp = tmp->next;
    }
    ret.reserve(n);
    tmp = _interfaces_ptr;
    while (tmp)
    {
        ret.push_back({tmp});
        tmp = tmp->next;
    }
    return ret;
}

std::vector<std::string> PcapInterfaces::names()
{
    std::vector<std::string> ret;
    pcap_if_t *tmp = _interfaces_ptr;
    int n = 0;
    while (tmp)
    {
        ++n;
        tmp = tmp->next;
    }
    ret.reserve(n);
    tmp = _interfaces_ptr;
    while (tmp)
    {
        ret.push_back(tmp->name);
        tmp = tmp->next;
    }
    return ret;
}

bool PcapInterfaces::find()
{
    this->_free();
    char errbuf[PCAP_ERRBUF_SIZE];
    _code = pcap_findalldevs(&_interfaces_ptr, errbuf);
    if (_code != 0)
        SIHD_LOG(error, "Interfaces: {}", errbuf);
    return _code == 0;
}

bool PcapInterfaces::error()
{
    return _code != 0;
}

std::string PcapInterfaces::status()
{
    return utils::status_str(_code);
}

// IFACE

PcapIFace::PcapIFace(pcap_if_t *ptr): _if_ptr(ptr)
{
    struct pcap_addr *tmp = _if_ptr->addresses;
    int n = 0;
    while (tmp)
    {
        ++n;
        tmp = tmp->next;
    }
    tmp = _if_ptr->addresses;
    _addr.reserve(n);
    while (tmp)
    {
        _addr.push_back(tmp);
        tmp = tmp->next;
    }
}

PcapIFace::~PcapIFace() {}

const std::vector<struct pcap_addr *> & PcapIFace::addresses() const
{
    return _addr;
};

std::string PcapIFace::dump() const
{
    std::string ret = _if_ptr->name;
    if (_if_ptr->description != nullptr)
    {
        ret += " - ";
        ret += _if_ptr->description;
    }
    ret += " (";
    if (this->loopback())
        ret += "loopback, ";
    if (this->up())
        ret += "up, ";
    if (this->wireless())
        ret += "wireless, ";
    if (this->can_be_connected())
    {
        if (this->connected())
            ret += "connected, ";
        else if (this->disconnected())
            ret += "disconnected, ";
        else if (this->connection_unknown())
            ret += "connection_unknown, ";
    }
    else
        ret += "no_connect, ";
    ret += std::to_string(_addr.size()) + " addresses)";
    return ret;
}

std::string PcapIFace::name() const
{
    return _if_ptr->name;
}

std::string PcapIFace::description() const
{
    return _if_ptr->description;
}

bool PcapIFace::loopback() const
{
    return _if_ptr->flags & PCAP_IF_LOOPBACK;
}

bool PcapIFace::up() const
{
    return _if_ptr->flags & PCAP_IF_UP;
}

bool PcapIFace::running() const
{
    return _if_ptr->flags & PCAP_IF_RUNNING;
}

bool PcapIFace::wireless() const
{
    return _if_ptr->flags & PCAP_IF_WIRELESS;
}

bool PcapIFace::connection_unknown() const
{
    return _if_ptr->flags & PCAP_IF_CONNECTION_STATUS_UNKNOWN;
}

bool PcapIFace::connected() const
{
    return _if_ptr->flags & PCAP_IF_CONNECTION_STATUS_CONNECTED;
}

bool PcapIFace::disconnected() const
{
    return _if_ptr->flags & PCAP_IF_CONNECTION_STATUS_DISCONNECTED;
}

bool PcapIFace::can_be_connected() const
{
    return !(_if_ptr->flags & PCAP_IF_CONNECTION_STATUS_NOT_APPLICABLE);
}

} // namespace sihd::pcap
