#include <sihd/net/IpAddr.hpp>
#include <sihd/util/Logger.hpp>

#include <strings.h>
#include <string.h>
#include <errno.h>

namespace sihd::net
{

LOGGER;

char IpAddr::_ip_buffer[INET6_ADDRSTRLEN];

IpAddr::IpAddr(const std::string & host, int port, bool dns_lookup): _host(host), _port(port)
{
    if (dns_lookup)
        this->do_lookup_dns(true);
    else
        this->_add_ip(host, 0, IPPROTO_IP);
}

IpAddr::IpAddr(const sockaddr_in & addr, bool dns_lookup)
{
    _host = IpAddr::ip_to_string(addr);
    _port = ntohs(addr.sin_port);
    if (dns_lookup)
        this->do_lookup_dns(true);
    else
        this->_add_ip(addr, 0, IPPROTO_IP);
}

IpAddr::IpAddr(const sockaddr_in6 & addr, bool dns_lookup)
{
    _host = IpAddr::ip_to_string(addr);
    _port = ntohs(addr.sin6_port);
    if (dns_lookup)
        this->do_lookup_dns(true);
    else
        this->_add_ip(addr, 0, IPPROTO_IP);
}

IpAddr::~IpAddr()
{
}

// Class methods

void    IpAddr::_fill_dns_lookup_hints(struct addrinfo *hints)
{
	memset(hints, 0, sizeof(struct addrinfo));
	hints->ai_family = AF_UNSPEC;
	hints->ai_socktype = 0;
	hints->ai_protocol = 0;
	hints->ai_flags = AI_CANONNAME;
}

IpAddr::IpInfo     IpAddr::_from_sockaddr(const sockaddr_in *addr_in, int socktype, int protocol)
{
    IpInfo ipinfo;
    ipinfo.ipv6 = false;
    ipinfo.socktype = socktype;
    ipinfo.protocol = protocol;
    memcpy(&ipinfo.addr, &addr_in->sin_addr, sizeof(in_addr));
    memset(&ipinfo.addr6, 0, sizeof(in6_addr));
    ipinfo.ip = IpAddr::ip_to_string(*addr_in);
    return ipinfo;
}

IpAddr::IpInfo     IpAddr::_from_sockaddr(const sockaddr_in6 *addr_in, int socktype, int protocol)
{
    IpInfo ipinfo;
    ipinfo.ipv6 = true;
    ipinfo.socktype = socktype;
    ipinfo.protocol = protocol;
    memset(&ipinfo.addr, 0, sizeof(in_addr));
    memcpy(&ipinfo.addr6, &addr_in->sin6_addr, sizeof(in6_addr));
    ipinfo.ip = IpAddr::ip_to_string(*addr_in);
    return ipinfo;
}

std::optional<IpAddr::IpInfo> IpAddr::_from_addrinfo(addrinfo *info, bool ipv6)
{
    if (ipv6 && info->ai_family == AF_INET6)
    {
        sockaddr_in6 *addr_in = (sockaddr_in6 *)info->ai_addr;
        return IpAddr::_from_sockaddr(addr_in, info->ai_socktype, info->ai_protocol);
    }
    else if (info->ai_family == AF_INET)
    {
        sockaddr_in *addr_in = (sockaddr_in *)info->ai_addr;
        return IpAddr::_from_sockaddr(addr_in, info->ai_socktype, info->ai_protocol);
    }
    return std::nullopt;
}

std::string IpAddr::fetch_ip_name(const std::string & ip)
{
    IpSockAddr ipsockaddr;

    if (IpAddr::fill_sockaddr(ip, &ipsockaddr) == false)
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
        LOG(error, "IpAddr: getnameinfo error: " << gai_strerror(ret));
        return "";
    }
    return host;
}

std::optional<IpAddr::DnsInfo> IpAddr::dns_lookup(const std::string & host, bool ipv6)
{
    int ret;
    struct addrinfo hints;
    struct addrinfo *results;
    struct addrinfo *first_result;

    IpAddr::_fill_dns_lookup_hints(&hints);
    if ((ret = getaddrinfo(host.c_str(), NULL, &hints, &results)) != 0)
    {
        LOG(error, "IpAddr: getaddrinfo error '" << host << "': " << gai_strerror(ret));
        return std::nullopt;
    }
    first_result = results;
    DnsInfo dns;
    dns.resolved = true;
    while (results)
    {
        // cannoname is only in first result
        if (results == first_result && results->ai_canonname)
            dns.hostname = results->ai_canonname;
        auto opt_info = IpAddr::_from_addrinfo(results, ipv6);
        if (opt_info)
            dns.lst_ip.push_back(opt_info.value());
        results = results->ai_next;
    }
    freeaddrinfo(first_result);
    return dns;
}

bool    IpAddr::fill_sockaddr(const std::string & ip, IpSockAddr *ipsockaddr, int port)
{
    if (IpAddr::to_sockaddr_in(&ipsockaddr->addr_in, ip, port))
    {
        ipsockaddr->addr = (sockaddr *)&ipsockaddr->addr_in;
        ipsockaddr->addr_len = sizeof(sockaddr_in);
        ipsockaddr->type = AF_INET;
    }
    else if (IpAddr::to_sockaddr_in6(&ipsockaddr->addr_in6, ip, port))
    {
        ipsockaddr->addr = (sockaddr *)&ipsockaddr->addr_in6;
        ipsockaddr->addr_len = sizeof(sockaddr_in6);
        ipsockaddr->type = AF_INET6;
    }
    return ipsockaddr->addr != nullptr;
}

bool   IpAddr::to_sockaddr_in(sockaddr_in *addr, const std::string & ip, int port)
{
    // memset(addr, 0, sizeof(sockaddr_in));
    int ret = inet_pton(AF_INET, ip.c_str(), &addr->sin_addr);
    if (ret <= 0)
    {
        // 0 is returned if src does not contain a character string representing a valid network address in the specified address family
        if (ret == -1)
            LOG(error, "IpAddr: to_sockaddr_in error for ip '" << ip << "': " << strerror(errno));
        return false;
    }
    addr->sin_family = AF_INET;
    addr->sin_port = htons(port);
    return true;
}

bool  IpAddr::to_sockaddr_in6(sockaddr_in6 *addr, const std::string & ip, int port)
{
    // memset(addr, 0, sizeof(sockaddr_in6));
    int ret = inet_pton(AF_INET6, ip.c_str(), &addr->sin6_addr);
    if (ret <= 0)
    {
        // 0 is returned if src does not contain a character string representing a valid network address in the specified address family
        if (ret == -1)
            LOG(error, "IpAddr: to_sockaddr_in6 error for ip '" << ip << "': " << strerror(errno));
        return false;
    }
    addr->sin6_family = AF_INET6;
    addr->sin6_port = htons(port);
    return true;
}

bool    IpAddr::is_valid_ipv4(const std::string & ip)
{
    struct sockaddr_in sa;
    return inet_pton(AF_INET, ip.c_str(), &(sa.sin_addr)) == 1;
}

bool    IpAddr::is_valid_ipv6(const std::string & ip)
{
    struct sockaddr_in6 sa;
    return inet_pton(AF_INET6, ip.c_str(), &(sa.sin6_addr)) == 1;
}

bool    IpAddr::is_valid_ip(const std::string & ip)
{
    return IpAddr::is_valid_ipv4(ip) || IpAddr::is_valid_ipv6(ip);
}

std::string     IpAddr::ip_to_string(const sockaddr_in & addr_in)
{
    return inet_ntop(AF_INET, (void *)&(addr_in.sin_addr), _ip_buffer, INET_ADDRSTRLEN);
}

std::string     IpAddr::ip_to_string(const sockaddr_in6 & addr_in)
{
    return inet_ntop(AF_INET6, (void *)&(addr_in.sin6_addr), _ip_buffer, INET6_ADDRSTRLEN);
}

// Instance methods

bool  IpAddr::get_sockaddr_in(sockaddr_in *addr, int socktype, int protocol) const
{
    const IpInfo *info = this->_get_ip_info(socktype, protocol, false);
    if (info != nullptr)
    {
        memset(addr, 0, sizeof(sockaddr_in));
        addr->sin_family = AF_INET;
        addr->sin_port = _port;
        memcpy(&addr->sin_addr, &info->addr, sizeof(in_addr));
    }
    return info != nullptr;
}

bool  IpAddr::get_sockaddr_in6(sockaddr_in6 *addr, int socktype, int protocol) const
{
    const IpInfo *info = this->_get_ip_info(socktype, protocol, true);
    if (info != nullptr)
    {
        memset(addr, 0, sizeof(sockaddr_in6));
        addr->sin6_family = AF_INET6;
        addr->sin6_port = _port;
        memcpy(&addr->sin6_addr, &info->addr6, sizeof(in6_addr));
    }
    return info != nullptr;
}

std::string IpAddr::get_ip(int socktype, int protocol, bool ipv6) const
{
    const IpInfo *info = this->_get_ip_info(socktype, protocol, ipv6);
    if (info != nullptr)
        return info->ip;
    return "";
}

bool    IpAddr::do_lookup_dns(bool ipv6)
{
    auto opt_dns = IpAddr::dns_lookup(_host, ipv6);
    if (opt_dns)
    {
        _dns = opt_dns.value();
        // if dns returned an IP as hostname
        if (IpAddr::is_valid_ip(_dns.hostname))
            _dns.hostname = IpAddr::fetch_ip_name(_dns.hostname);
        _dns.resolved = true;
    }
    return opt_dns.has_value();
}

// Private instance methods

IpAddr::IpInfo *IpAddr::_get_ip_info(int socktype, int protocol, bool ipv6) const
{
    for (const IpInfo & info: _dns.lst_ip)
    {
        if (ipv6 && !info.ipv6)
            continue ;
        if ((socktype < 0 || info.socktype == socktype)
            && (protocol < 0 || info.protocol == protocol)
            && info.ip.empty() == false)
            return const_cast<IpInfo *>(&info);
    }
    return nullptr;
}

void    IpAddr::_add_ip(const std::string & ip, int socktype, int protocol)
{
    for (const auto & info: _dns.lst_ip)
    {
        if (info.ip == ip && info.socktype == socktype && info.protocol == protocol)
            return ;
    }
    sockaddr_in addr_in;
    sockaddr_in6 addr_in6;
    if (IpAddr::to_sockaddr_in6(&addr_in6, ip))
    {
        IpInfo ipinfo = IpAddr::_from_sockaddr(&addr_in6, socktype, protocol);
        _dns.lst_ip.push_back(ipinfo);
    }
    else if (IpAddr::to_sockaddr_in(&addr_in, ip))
    {
        IpInfo ipinfo = IpAddr::_from_sockaddr(&addr_in, socktype, protocol);
        _dns.lst_ip.push_back(ipinfo);
    }
}

void    IpAddr::_add_ip(const sockaddr_in6 & addr, int socktype, int protocol)
{
    for (const auto & info: _dns.lst_ip)
    {
        if (memcmp(&info.addr6, &addr, sizeof(in6_addr)) == 0 && info.socktype == socktype && info.protocol == protocol)
            return ;
    }
    IpInfo ipinfo = IpAddr::_from_sockaddr(&addr, socktype, protocol);
    _dns.lst_ip.push_back(ipinfo);
}

void    IpAddr::_add_ip(const sockaddr_in & addr, int socktype, int protocol)
{
    for (const auto & info: _dns.lst_ip)
    {
        if (memcmp(&info.addr, &addr, sizeof(in_addr)) == 0 && info.socktype == socktype && info.protocol == protocol)
            return ;
    }
    IpInfo ipinfo = IpAddr::_from_sockaddr(&addr, socktype, protocol);
    _dns.lst_ip.push_back(ipinfo);
}

}