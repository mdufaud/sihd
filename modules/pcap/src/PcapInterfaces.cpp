#include <pcap.h>

#include <sihd/pcap/PcapInterfaces.hpp>
#include <sihd/pcap/utils.hpp>
#include <sihd/util/Logger.hpp>

namespace sihd::pcap
{

SIHD_NEW_LOGGER("sihd::pcap");

namespace
{

sihd::net::IpAddr make_ip(const struct sockaddr *addr)
{
    if (addr == nullptr)
        return {};
    return sihd::net::IpAddr(*addr);
}

std::vector<PcapAddress> make_addresses(const struct pcap_addr *pcap_addr_ptr)
{
    std::vector<PcapAddress> ret;
    const struct pcap_addr *tmp = pcap_addr_ptr;
    while (tmp)
    {
        ret.push_back({
            .addr = make_ip(tmp->addr),
            .netmask = make_ip(tmp->netmask),
            .broadaddr = make_ip(tmp->broadaddr),
            .dstaddr = make_ip(tmp->dstaddr),
        });
        tmp = tmp->next;
    }
    return ret;
}

} // namespace

// PcapInterfaces

struct PcapInterfaces::Impl
{
        int code {0};
        pcap_if_t *interfaces_ptr {nullptr};

        void free()
        {
            if (interfaces_ptr != nullptr)
            {
                pcap_freealldevs(interfaces_ptr);
                interfaces_ptr = nullptr;
            }
        }
};

PcapInterfaces::PcapInterfaces(): _impl_ptr(std::make_unique<Impl>())
{
    utils::init();
    this->find();
}

PcapInterfaces::~PcapInterfaces()
{
    _impl_ptr->free();
}

std::vector<PcapIFace> PcapInterfaces::ifaces()
{
    std::vector<PcapIFace> ret;
    pcap_if_t *tmp = _impl_ptr->interfaces_ptr;
    while (tmp)
    {
        ret.emplace_back(tmp->name ? tmp->name : "",
                         tmp->description ? tmp->description : "",
                         tmp->flags,
                         make_addresses(tmp->addresses));
        tmp = tmp->next;
    }
    return ret;
}

std::vector<std::string> PcapInterfaces::names()
{
    std::vector<std::string> ret;
    pcap_if_t *tmp = _impl_ptr->interfaces_ptr;
    while (tmp)
    {
        ret.emplace_back(tmp->name);
        tmp = tmp->next;
    }
    return ret;
}

bool PcapInterfaces::find()
{
    _impl_ptr->free();
    char errbuf[PCAP_ERRBUF_SIZE];
    _impl_ptr->code = pcap_findalldevs(&_impl_ptr->interfaces_ptr, errbuf);
    if (_impl_ptr->code != 0)
        SIHD_LOG(error, "Interfaces: {}", errbuf);
    return _impl_ptr->code == 0;
}

bool PcapInterfaces::error()
{
    return _impl_ptr->code != 0;
}

std::string PcapInterfaces::status()
{
    return utils::status_str(_impl_ptr->code);
}

// PcapIFace

PcapIFace::PcapIFace(std::string name, std::string description, uint32_t flags, std::vector<PcapAddress> addresses):
    _name(std::move(name)),
    _description(std::move(description)),
    _flags(flags),
    _addresses(std::move(addresses))
{
}

PcapIFace::~PcapIFace() = default;

const std::vector<PcapAddress> & PcapIFace::addresses() const
{
    return _addresses;
}

std::string PcapIFace::dump() const
{
    std::string ret = _name;
    if (!_description.empty())
    {
        ret += " - ";
        ret += _description;
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
    ret += std::to_string(_addresses.size()) + " addresses)";
    return ret;
}

const std::string & PcapIFace::name() const
{
    return _name;
}

const std::string & PcapIFace::description() const
{
    return _description;
}

bool PcapIFace::loopback() const
{
    return _flags & PCAP_IF_LOOPBACK;
}

bool PcapIFace::up() const
{
    return _flags & PCAP_IF_UP;
}

bool PcapIFace::running() const
{
    return _flags & PCAP_IF_RUNNING;
}

bool PcapIFace::wireless() const
{
    return _flags & PCAP_IF_WIRELESS;
}

bool PcapIFace::connection_unknown() const
{
    return _flags & PCAP_IF_CONNECTION_STATUS_UNKNOWN;
}

bool PcapIFace::connected() const
{
    return _flags & PCAP_IF_CONNECTION_STATUS_CONNECTED;
}

bool PcapIFace::disconnected() const
{
    return _flags & PCAP_IF_CONNECTION_STATUS_DISCONNECTED;
}

bool PcapIFace::can_be_connected() const
{
    return !(_flags & PCAP_IF_CONNECTION_STATUS_NOT_APPLICABLE);
}

} // namespace sihd::pcap
