#include <strings.h>

#include <bitset>
#include <cerrno>
#include <cstring>

#include <sihd/util/Logger.hpp>
#include <sihd/util/str.hpp>

#include <sihd/net/IpAddr.hpp>

#if !defined(__SIHD_WINDOWS__)
# include <arpa/inet.h>  // inet_pton...
# include <netdb.h>      // getnameinfo
# include <netinet/in.h> // sockaddr
# include <sys/socket.h> // getnameinfo
#else
# include <winsock2.h>
# include <ws2tcpip.h> // addrinfo
#endif

namespace sihd::net
{

SIHD_NEW_LOGGER("sihd::net");

using namespace sihd::util;

char IpAddr::_ip_buffer[INET6_ADDRSTRLEN];

IpAddr::IpAddr()
{
    this->clear();
}

IpAddr::IpAddr(int port, bool ipv6)
{
    this->from_any(port, ipv6);
}

IpAddr::IpAddr(std::string_view host, bool dns_lookup): IpAddr(host, 0, dns_lookup) {}

IpAddr::IpAddr(std::string_view host, int port, bool dns_lookup)
{
    if (this->from(host, port) && dns_lookup)
        this->do_lookup_dns();
}

IpAddr::IpAddr(const sockaddr & addr, size_t addr_len, bool dns_lookup)
{
    if (this->from(addr, addr_len) && dns_lookup)
        this->do_lookup_dns();
}

IpAddr::IpAddr(const sockaddr & addr, bool dns_lookup)
{
    if (this->from(addr, 0) && dns_lookup)
        this->do_lookup_dns();
}

IpAddr::IpAddr(const sockaddr_in & addr, bool dns_lookup)
{
    if (this->from(addr) && dns_lookup)
        this->do_lookup_dns();
}

IpAddr::IpAddr(const sockaddr_in6 & addr, bool dns_lookup)
{
    if (this->from(addr) && dns_lookup)
        this->do_lookup_dns();
}

IpAddr::~IpAddr() {}

void IpAddr::from_any(int port, bool ipv6)
{
    this->clear();
    _port = port;
    if (ipv6)
    {
        sockaddr_in6 addr_in6;
        memset(&addr_in6, 0, sizeof(addr_in6));
        addr_in6.sin6_family = AF_INET6;
        addr_in6.sin6_port = htons(port);
        addr_in6.sin6_addr = IN6ADDR_ANY_INIT;
        this->_add_ip(addr_in6, 0, IPPROTO_IP);
    }
    else
    {
        sockaddr_in addr_in;
        memset(&addr_in, 0, sizeof(addr_in));
        addr_in.sin_family = AF_INET;
        addr_in.sin_port = htons(port);
        addr_in.sin_addr.s_addr = INADDR_ANY;
        this->_add_ip(addr_in, 0, IPPROTO_IP);
    }
}

bool IpAddr::from(std::string_view host, int port)
{
    this->clear();
    size_t idx = host.find('/');
    if (idx != std::string::npos)
    {
        _host = host.substr(0, idx);
        this->_netmask_from_str(host.substr(idx + 1));
    }
    else
    {
        _host = host;
    }
    _port = port;
    this->_add_ip(_host, 0, IPPROTO_IP);
    if (this->is_valid_ipv6(_host))
        this->set_ipv6_preference(true);
    this->_fill_subnet();
    return true;
}

bool IpAddr::from(const sockaddr & addr, size_t addr_len)
{
    sockaddr_in addr_in;
    sockaddr_in6 addr_in6;

    this->clear();
    if (IpAddr::to_sockaddr_in6(&addr_in6, addr, addr_len))
    {
        _host = IpAddr::ip_str(addr_in6);
        _port = ntohs(addr_in6.sin6_port);
        this->_add_ip(addr_in6, 0, IPPROTO_IP);
        this->set_ipv6_preference(true);
    }
    else if (IpAddr::to_sockaddr_in(&addr_in, addr, addr_len))
    {
        _host = IpAddr::ip_str(addr_in);
        _port = ntohs(addr_in.sin_port);
        this->_add_ip(addr_in, 0, IPPROTO_IP);
    }
    return true;
}

bool IpAddr::from(const sockaddr_in & addr_in)
{
    this->clear();
    _host = IpAddr::ip_str(addr_in);
    _port = ntohs(addr_in.sin_port);
    this->_add_ip(addr_in, 0, IPPROTO_IP);
    return true;
}

bool IpAddr::from(const sockaddr_in6 & addr_in6)
{
    this->clear();
    _host = IpAddr::ip_str(addr_in6);
    _port = ntohs(addr_in6.sin6_port);
    this->_add_ip(addr_in6, 0, IPPROTO_IP);
    this->set_ipv6_preference(true);
    return true;
}

void IpAddr::clear()
{
    _host.clear();
    _port = -1;
    _prefers_ipv6 = false;
    _hostname.clear();
    _dns_resolved = false;
    _lst_ip.clear();
    this->_reset_subnet();
}

/* ************************************************************************* */
/* Subnet utilities */
/* ************************************************************************* */

void IpAddr::_reset_subnet()
{
    _has_subnet = false;
    memset(&_subnet, 0, sizeof(Subnet));
}

void IpAddr::_fill_subnet()
{
    if (_has_subnet == false)
        return;
    struct sockaddr_in addr;
    if (this->get_first_sockaddr_in(&addr) == false)
    {
        SIHD_LOG(warning, "IpAddr: no IPV4 address found to fill subnet for host: {}", _host);
        return;
    }
    _subnet.netid.s_addr = addr.sin_addr.s_addr & _subnet.netmask.s_addr;
    _subnet.wildcard.s_addr = ~_subnet.netmask.s_addr;
    _subnet.broadcast.s_addr = _subnet.netid.s_addr | _subnet.wildcard.s_addr;
    _subnet.hostmin.s_addr = ntohl(htonl(_subnet.netid.s_addr) + 1);
    _subnet.hostmax.s_addr = ntohl(htonl(_subnet.broadcast.s_addr) - 1);
    _subnet.hosts = htonl(_subnet.wildcard.s_addr) - 1;
}

std::string IpAddr::dump_subnet() const
{
    return fmt::format("network id: {}\nwildcard: {}\nnetmask: {}\nhost min: {}\n"
                       "host max: {}\nbroadcast: {}\nnumber of hosts: {}\n",
                       IpAddr::ip_str(_subnet.netid),
                       IpAddr::ip_str(_subnet.wildcard),
                       IpAddr::ip_str(_subnet.netmask),
                       IpAddr::ip_str(_subnet.hostmin),
                       IpAddr::ip_str(_subnet.hostmax),
                       IpAddr::ip_str(_subnet.broadcast),
                       _subnet.hosts);
}

size_t IpAddr::subnet_value() const
{
    return std::bitset<32>(_subnet.netmask.s_addr).count();
}

bool IpAddr::_netmask_from_str(std::string_view mask_value_str)
{
    uint32_t mask_value;
    if (!str::is_number(mask_value_str) || !str::convert_from_string<uint32_t>(mask_value_str, mask_value))
    {
        SIHD_LOG(error, "IpAddr: not a subnet mask: {}", mask_value_str);
        return false;
    }
    uint32_t actual_mask = IpAddr::to_netmask(mask_value);
    if (IpAddr::is_valid_netmask(htonl(actual_mask)) == false)
    {
        SIHD_LOG(error, "IpAddr: not a valid mask: {}", mask_value_str);
        return false;
    }
    _subnet.netmask.s_addr = actual_mask;
    _has_subnet = true;
    return true;
}

bool IpAddr::set_subnet_mask(std::string_view mask)
{
    struct sockaddr_in sockaddr_mask;
    if (IpAddr::to_sockaddr_in(&sockaddr_mask, mask) == false)
        return false;
    if (IpAddr::is_valid_netmask(htonl(sockaddr_mask.sin_addr.s_addr)) == false)
    {
        SIHD_LOG(error, "IpAddr: not a valid mask: {}", mask);
        return false;
    }
    _subnet.netmask.s_addr = sockaddr_mask.sin_addr.s_addr;
    _has_subnet = true;
    this->_fill_subnet();
    return true;
}

bool IpAddr::set_subnet_mask(uint32_t mask)
{
    if (IpAddr::is_valid_netmask(htonl(mask)) == false)
    {
        SIHD_LOG(error, "IpAddr: not a valid mask: {}", mask);
        return false;
    }
    _subnet.netmask.s_addr = mask;
    _has_subnet = true;
    this->_fill_subnet();
    return true;
}

bool IpAddr::is_same_subnet(std::string_view ip) const
{
    IpAddr addr(ip);
    return this->is_same_subnet(addr);
}

bool IpAddr::is_same_subnet(const IpAddr & addr) const
{
    struct sockaddr_in to_check;
    return addr.get_first_sockaddr_in(&to_check) && this->is_same_subnet(to_check);
}

bool IpAddr::is_same_subnet(const sockaddr_in & addr) const
{
    return (addr.sin_addr.s_addr & _subnet.netmask.s_addr) == (_subnet.netid.s_addr & _subnet.netmask.s_addr);
}

/* ************************************************************************* */
/* Static class utilities */
/* ************************************************************************* */

bool IpAddr::fill_ipsockaddr(IpSockAddr & ipsockaddr, std::string_view ip, int port)
{
    IpAddr::_purge_ipsockaddr(ipsockaddr);
    if (IpAddr::to_sockaddr_in(&ipsockaddr.addr_in, ip, port))
    {
        ipsockaddr.addr = (sockaddr *)&ipsockaddr.addr_in;
        ipsockaddr.addr_len = sizeof(sockaddr_in);
        ipsockaddr.type = AF_INET;
    }
    else if (IpAddr::to_sockaddr_in6(&ipsockaddr.addr_in6, ip, port))
    {
        ipsockaddr.addr = (sockaddr *)&ipsockaddr.addr_in6;
        ipsockaddr.addr_len = sizeof(sockaddr_in6);
        ipsockaddr.type = AF_INET6;
    }
    return ipsockaddr.addr != nullptr;
}

bool IpAddr::to_sockaddr_in(sockaddr_in *filled, const sockaddr & addr, size_t addr_len)
{
    if (addr.sa_family == AF_INET || addr_len == sizeof(sockaddr_in))
    {
        memcpy(filled, &addr, sizeof(sockaddr_in));
        return true;
    }
    return false;
}

bool IpAddr::to_sockaddr_in6(sockaddr_in6 *filled, const sockaddr & addr, size_t addr_len)
{
    if (addr.sa_family == AF_INET6 || addr_len == sizeof(sockaddr_in6))
    {
        memcpy(filled, &addr, sizeof(sockaddr_in6));
        return true;
    }
    return false;
}

bool IpAddr::to_sockaddr_in(sockaddr_in *addr, std::string_view ip, int port)
{
    // memset(addr, 0, sizeof(sockaddr_in));
    int ret = inet_pton(AF_INET, ip.data(), &addr->sin_addr);
    if (ret <= 0)
    {
        // 0 is returned if src does not contain a character string representing a valid network address in the
        // specified address family
        if (ret == -1)
            SIHD_LOG(error, "IpAddr: to_sockaddr_in error for ip '{}': {}", ip, strerror(errno));
        return false;
    }
    addr->sin_family = AF_INET;
    addr->sin_port = htons(port);
    return true;
}

bool IpAddr::to_sockaddr_in6(sockaddr_in6 *addr, std::string_view ip, int port)
{
    // memset(addr, 0, sizeof(sockaddr_in6));
    int ret = inet_pton(AF_INET6, ip.data(), &addr->sin6_addr);
    if (ret <= 0)
    {
        // 0 is returned if src does not contain a character string representing a valid network address in the
        // specified address family
        if (ret == -1)
            SIHD_LOG(error, "IpAddr: to_sockaddr_in6 error for ip '{}': {}", ip, strerror(errno));
        return false;
    }
    addr->sin6_family = AF_INET6;
    addr->sin6_port = htons(port);
    return true;
}

IpAddr IpAddr::localhost(int port, bool ipv6)
{
    return {ipv6 ? "::1" : "127.0.0.1", port, false};
}

std::string IpAddr::fetch_ip_name(std::string_view ip)
{
    IpSockAddr ipsockaddr;

    if (IpAddr::fill_ipsockaddr(ipsockaddr, ip) == false)
        return "";
    return IpAddr::fetch_ip_name(ipsockaddr.addr, ipsockaddr.addr_len);
}

std::string IpAddr::fetch_ip_name(sockaddr *addr, socklen_t addr_len)
{
    char host[NI_MAXHOST];
    int flags = NI_NAMEREQD;
    int ret = ::getnameinfo(addr, addr_len, host, NI_MAXHOST, nullptr, 0, flags);
    if (ret != 0)
    {
        SIHD_LOG(error, "IpAddr: getnameinfo error: {}", gai_strerror(ret));
        return "";
    }
    return host;
}

bool IpAddr::is_valid_ipv4(std::string_view ip)
{
    struct sockaddr_in sa;
    return inet_pton(AF_INET, ip.data(), &(sa.sin_addr)) == 1;
}

bool IpAddr::is_valid_ipv6(std::string_view ip)
{
    struct sockaddr_in6 sa;
    return inet_pton(AF_INET6, ip.data(), &(sa.sin6_addr)) == 1;
}

bool IpAddr::is_valid_ip(std::string_view ip)
{
    return IpAddr::is_valid_ipv4(ip) || IpAddr::is_valid_ipv6(ip);
}

std::string IpAddr::ipv4_str(const sockaddr & addr)
{
    return IpAddr::ip_str(reinterpret_cast<const sockaddr_in &>(addr));
}

std::string IpAddr::ipv6_str(const sockaddr & addr)
{
    return IpAddr::ip_str(reinterpret_cast<const sockaddr_in6 &>(addr));
}

std::string IpAddr::ip_str(const sockaddr & addr)
{
    sockaddr_in addr_in;
    sockaddr_in6 addr_in6;

    if (IpAddr::to_sockaddr_in6(&addr_in6, addr, 0))
        return IpAddr::ip_str(addr_in6);
    else if (IpAddr::to_sockaddr_in(&addr_in, addr, 0))
        return IpAddr::ip_str(addr_in);
    return "";
}

std::string IpAddr::ip_str(const sockaddr_in & addr_in)
{
    return IpAddr::ip_str(addr_in.sin_addr);
}

std::string IpAddr::ip_str(const sockaddr_in6 & addr_in)
{
    return IpAddr::ip_str(addr_in.sin6_addr);
}

std::string IpAddr::ip_str(const in_addr & addr_in)
{
    return inet_ntop(AF_INET, (void *)(&addr_in), _ip_buffer, INET_ADDRSTRLEN);
}

std::string IpAddr::ip_str(const in6_addr & addr_in)
{
    return inet_ntop(AF_INET6, (void *)(&addr_in), _ip_buffer, INET6_ADDRSTRLEN);
}

void IpAddr::fill_sockaddr_network_id(struct sockaddr_in & dst, struct sockaddr_in & src, in_addr mask)
{
    memcpy(&dst, &src, sizeof(struct sockaddr_in));
    dst.sin_addr.s_addr = src.sin_addr.s_addr & mask.s_addr;
}

void IpAddr::fill_sockaddr_network_id(struct sockaddr_in6 & dst, struct sockaddr_in6 & src, in6_addr mask)
{
    memcpy(&dst, &src, sizeof(struct sockaddr_in6));
    for (int i = 0; i < 16; ++i)
    {
        dst.sin6_addr.s6_addr[i] = src.sin6_addr.s6_addr[i] & mask.s6_addr[i];
    }
}

void IpAddr::fill_sockaddr_broadcast(struct sockaddr_in & dst, struct sockaddr_in & src, in_addr mask)
{
    memcpy(&dst, &src, sizeof(struct sockaddr_in));
    dst.sin_addr.s_addr = src.sin_addr.s_addr | ~(mask.s_addr);
}

void IpAddr::fill_sockaddr_broadcast(struct sockaddr_in6 & dst, struct sockaddr_in6 & src, in6_addr mask)
{
    memcpy(&dst, &src, sizeof(struct sockaddr_in6));
    for (int i = 0; i < 16; ++i)
    {
        dst.sin6_addr.s6_addr[i] = src.sin6_addr.s6_addr[i] | ~(mask.s6_addr[i]);
    }
}

uint32_t IpAddr::to_netmask(uint32_t value)
{
    return ntohl(0xffffffff << (32 - value));
}

// A valid netmask cannot have a zero with a one to the right of it. All zeros must have another zero to the right of it
// or be bit 0. must apply htonl if mask is in host byte order
bool IpAddr::is_valid_netmask(uint32_t mask)
{
    if (mask == 0)
        return false;
    uint32_t y = ~mask;
    uint32_t z = y + 1;
    return (z & y) == 0;
}

/* ************************************************************************* */
/* DNS lookup */
/* ************************************************************************* */

bool IpAddr::_from_addrinfo(IpAddr::IpEntry *entry, addrinfo *info, bool ipv6)
{
    if (ipv6 && info->ai_family == AF_INET6)
    {
        sockaddr_in6 *addr_in = (sockaddr_in6 *)info->ai_addr;
        IpAddr::_from_sockaddr(entry, addr_in, info->ai_socktype, info->ai_protocol);
    }
    else if (info->ai_family == AF_INET)
    {
        sockaddr_in *addr_in = (sockaddr_in *)info->ai_addr;
        IpAddr::_from_sockaddr(entry, addr_in, info->ai_socktype, info->ai_protocol);
    }
    else
        return false;
    return true;
}

void IpAddr::_fill_dns_lookup_hints(struct addrinfo *hints)
{
    memset(hints, 0, sizeof(struct addrinfo));
    hints->ai_family = AF_UNSPEC;
    hints->ai_socktype = 0;
    hints->ai_protocol = 0;
    hints->ai_flags = AI_CANONNAME;
}

std::optional<IpAddr::DnsInfo> IpAddr::dns_lookup(std::string_view host, bool ipv6)
{
    int ret;
    struct addrinfo hints;
    struct addrinfo *results;
    struct addrinfo *first_result;

    IpAddr::_fill_dns_lookup_hints(&hints);
    if ((ret = getaddrinfo(host.data(), NULL, &hints, &results)) != 0)
    {
        SIHD_LOG(error, "IpAddr: getaddrinfo error '{}': {}", host, gai_strerror(ret));
        return std::nullopt;
    }
    first_result = results;
    DnsInfo dns;
    while (results)
    {
        // cannoname is only in first result
        if (results == first_result && results->ai_canonname)
            dns.hostname = results->ai_canonname;
        IpAddr::IpEntry entry;
        if (IpAddr::_from_addrinfo(&entry, results, ipv6))
            dns.lst_ip.push_back(entry);
        results = results->ai_next;
    }
    freeaddrinfo(first_result);
    return dns;
}

bool IpAddr::do_lookup_dns()
{
    auto opt_dns = IpAddr::dns_lookup(_host, true);
    if (opt_dns.has_value() == false)
        return false;
    DnsInfo info = opt_dns.value();
    // if dns returned an IP as hostname
    if (IpAddr::is_valid_ip(info.hostname))
        _hostname = IpAddr::fetch_ip_name(info.hostname);
    else
        _hostname = info.hostname;
    // add ip list if not already added
    for (const IpEntry & dns_entry : info.lst_ip)
    {
        this->_add_ip(dns_entry);
    }
    _dns_resolved = true;
    return true;
}

/* ************************************************************************* */
/* Find ip */
/* ************************************************************************* */

size_t IpAddr::ipv4_count() const
{
    size_t count = 0;

    for (const auto & ip_entry : _lst_ip)
    {
        if (ip_entry.ipv6 == false)
            ++count;
    }

    return count;
}

size_t IpAddr::ipv6_count() const
{
    size_t count = 0;

    for (const auto & ip_entry : _lst_ip)
    {
        if (ip_entry.ipv6)
            ++count;
    }

    return count;
}

bool IpAddr::get_sockaddr(IpSockAddr & ipsockaddr, int socktype, int protocol) const
{
    IpAddr::_purge_ipsockaddr(ipsockaddr);
    if (this->get_sockaddr_in6(&ipsockaddr.addr_in6, socktype, protocol))
    {
        ipsockaddr.type = AF_INET6;
        ipsockaddr.addr = (sockaddr *)&ipsockaddr.addr_in6;
        ipsockaddr.addr_len = sizeof(sockaddr_in6);
    }
    else if (this->get_sockaddr_in(&ipsockaddr.addr_in, socktype, protocol))
    {
        ipsockaddr.type = AF_INET;
        ipsockaddr.addr = (sockaddr *)&ipsockaddr.addr_in;
        ipsockaddr.addr_len = sizeof(sockaddr_in);
    }
    return ipsockaddr.addr != nullptr;
}

bool IpAddr::get_sockaddr_in(sockaddr_in *addr, int socktype, int protocol) const
{
    const IpAddr::IpEntry *info = this->_get_ip_info(socktype, protocol, false);
    if (info != nullptr)
    {
        memset(addr, 0, sizeof(sockaddr_in));
        addr->sin_family = AF_INET;
        addr->sin_port = htons(_port);
        memcpy(&addr->sin_addr, &info->addr, sizeof(in_addr));
    }
    return info != nullptr;
}

bool IpAddr::get_sockaddr_in6(sockaddr_in6 *addr, int socktype, int protocol) const
{
    const IpAddr::IpEntry *info = this->_get_ip_info(socktype, protocol, true);
    if (info != nullptr)
    {
        memset(addr, 0, sizeof(sockaddr_in6));
        addr->sin6_family = AF_INET6;
        addr->sin6_port = htons(_port);
        memcpy(&addr->sin6_addr, &info->addr6, sizeof(in6_addr));
    }
    return info != nullptr;
}

std::string IpAddr::matching_ip_str(std::string_view socktype, std::string_view protocol, bool ipv6) const
{
    return this->matching_ip_str(ip::socktype(socktype), ip::protocol(protocol), ipv6);
}

std::string IpAddr::matching_ip_str(int socktype, int protocol, bool ipv6) const
{
    const IpAddr::IpEntry *info = this->_get_ip_info(socktype, protocol, ipv6);
    if (info != nullptr)
        return info->ip();
    return "";
}

const IpAddr::IpEntry *IpAddr::_get_ip_info(int socktype, int protocol, bool ipv6) const
{
    for (const IpAddr::IpEntry & info : _lst_ip)
    {
        if (ipv6 != info.ipv6)
            continue;
        if ((socktype < 0 || info.socktype == socktype) && (protocol < 0 || info.protocol == protocol))
            return &info;
    }
    return nullptr;
}

/* ************************************************************************* */
/* Add IP entry */
/* ************************************************************************* */

void IpAddr::_from_sockaddr(IpAddr::IpEntry *entry, const sockaddr_in *addr_in, int socktype, int protocol)
{
    entry->ipv6 = false;
    entry->socktype = socktype;
    entry->protocol = protocol;
    memcpy(&entry->addr, &addr_in->sin_addr, sizeof(in_addr));
    memset(&entry->addr6, 0, sizeof(in6_addr));
}

void IpAddr::_from_sockaddr(IpAddr::IpEntry *entry, const sockaddr_in6 *addr_in, int socktype, int protocol)
{
    entry->ipv6 = true;
    entry->socktype = socktype;
    entry->protocol = protocol;
    memset(&entry->addr, 0, sizeof(in_addr));
    memcpy(&entry->addr6, &addr_in->sin6_addr, sizeof(in6_addr));
}

void IpAddr::_add_ip(const IpAddr::IpEntry & ip_entry)
{
    for (const auto & entry : _lst_ip)
    {
        if (entry.socktype == ip_entry.socktype && entry.protocol == ip_entry.protocol)
            return;
    }
    _lst_ip.push_back(ip_entry);
}

void IpAddr::_add_ip(std::string_view ip, int socktype, int protocol)
{
    for (const auto & info : _lst_ip)
    {
        if (info.socktype == socktype && info.protocol == protocol && info.ip() == ip)
            return;
    }
    sockaddr_in addr_in;
    sockaddr_in6 addr_in6;
    if (IpAddr::to_sockaddr_in6(&addr_in6, ip))
        this->_add_ip(addr_in6, socktype, protocol);
    else if (IpAddr::to_sockaddr_in(&addr_in, ip))
        this->_add_ip(addr_in, socktype, protocol);
}

void IpAddr::_add_ip(const sockaddr_in6 & addr, int socktype, int protocol)
{
    for (const auto & info : _lst_ip)
    {
        if (memcmp(&info.addr6, &addr, sizeof(in6_addr)) == 0 && info.socktype == socktype && info.protocol == protocol)
            return;
    }
    IpAddr::IpEntry entry;
    IpAddr::_from_sockaddr(&entry, &addr, socktype, protocol);
    _lst_ip.push_back(entry);
}

void IpAddr::_add_ip(const sockaddr_in & addr, int socktype, int protocol)
{
    for (const auto & info : _lst_ip)
    {
        if (memcmp(&info.addr, &addr, sizeof(in_addr)) == 0 && info.socktype == socktype && info.protocol == protocol)
            return;
    }
    IpAddr::IpEntry entry;
    IpAddr::_from_sockaddr(&entry, &addr, socktype, protocol);
    _lst_ip.push_back(entry);
}

std::string IpAddr::dump_ip_lst() const
{
    std::string dump = fmt::format("hostname: {}\nhost: {}\n", _hostname, _host);

    for (const IpEntry & entry : _lst_ip)
    {
        dump += fmt::format("ipv{} {} {}: {}\n",
                            (entry.ipv6 ? '6' : '4'),
                            ip::protocol_str(entry.protocol),
                            ip::socktype_str(entry.socktype),
                            entry.ip());
    }
    return dump;
}

/* ************************************************************************* */
/* Private utilities */
/* ************************************************************************* */

void IpAddr::_purge_ipsockaddr(IpSockAddr & ipsockaddr)
{
    memset(&ipsockaddr, 0, sizeof(IpSockAddr));
}

} // namespace sihd::net
