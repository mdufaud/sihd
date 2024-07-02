
#include <strings.h>

#include <bitset>
#include <cerrno>
#include <cstring>
#include <stdexcept>

#include <fmt/format.h>

#include <sihd/net/IpAddr.hpp>
#include <sihd/net/ip.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/platform.hpp>
#include <sihd/util/str.hpp>

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
using namespace sihd::net;

namespace
{

// TODO to ip.hpp
bool to_sockaddr_in(sockaddr_in *addr, std::string_view ip, int port = 0)
{
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

// TODO to ip.hpp
bool to_sockaddr_in6(sockaddr_in6 *addr, std::string_view ip, int port = 0)
{
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

// TODO to ip.hpp
uint32_t to_netmask(uint32_t value)
{
    return ntohl(0xffffffff << (32 - value));
}

// TODO to ip.hpp
//  A valid netmask cannot have a zero with a one to the right of it. All zeros must have another zero to the right of
//  it or be bit 0. must apply htonl if mask is in host byte order
bool is_valid_netmask(uint32_t mask)
{
    if (mask == 0)
        return false;
    uint32_t y = ~mask;
    uint32_t z = y + 1;
    return (z & y) == 0;
}

// TODO to ip.hpp
uint32_t netmask_value_from_str(std::string_view mask_value_str)
{
    uint32_t mask_value;
    if (!str::is_number(mask_value_str) || !str::convert_from_string<uint32_t>(mask_value_str, mask_value))
    {
        SIHD_LOG(error, "IpAddr: not a subnet mask: {}", mask_value_str);
        return -1;
    }
    return mask_value;
}

}; // namespace

IpAddr::IpAddr()
{
    memset(&_addr.sockaddr, 0, sizeof(struct sockaddr_in6));
    _port = 0;
    _netmask_value = 0;
}

IpAddr::IpAddr(int port, bool ipv6): IpAddr()
{
    _port = port;
    if (ipv6)
    {
        memset(&_addr.sockaddr_in6, 0, sizeof(_addr.sockaddr_in6));
        _addr.sockaddr_in6.sin6_family = AF_INET6;
        _addr.sockaddr_in6.sin6_port = htons(port);
        _addr.sockaddr_in6.sin6_addr = IN6ADDR_ANY_INIT;
    }
    else
    {
        memset(&_addr.sockaddr_in, 0, sizeof(_addr.sockaddr_in));
        _addr.sockaddr_in.sin_family = AF_INET;
        _addr.sockaddr_in.sin_port = htons(port);
        _addr.sockaddr_in.sin_addr.s_addr = INADDR_ANY;
    }
}

IpAddr::IpAddr(std::string_view host, int port): IpAddr()
{
    std::string ip;
    size_t idx = host.find('/');
    if (idx != std::string::npos)
    {
        ip = host.substr(0, idx);
        _netmask_value = netmask_value_from_str(host.substr(idx + 1));
    }
    else
    {
        ip = host;
    }

    if (!to_sockaddr_in(&_addr.sockaddr_in, ip, port))
    {
        to_sockaddr_in6(&_addr.sockaddr_in6, ip, port);
    }
}

IpAddr::IpAddr(std::string_view host): IpAddr(host, 0) {}

IpAddr::IpAddr(const sockaddr & addr, size_t addr_len): IpAddr()
{
    if (addr.sa_family == AF_INET && (addr_len == 0 || addr_len == sizeof(sockaddr_in)))
    {
        memcpy(&_addr.sockaddr, &addr, sizeof(sockaddr_in));
        _port = ntohs(_addr.sockaddr_in.sin_port);
    }
    else if (addr.sa_family == AF_INET6 && (addr_len == 0 || addr_len == sizeof(sockaddr_in6)))
    {
        memcpy(&_addr.sockaddr, &addr, sizeof(sockaddr_in));
        _port = ntohs(_addr.sockaddr_in6.sin6_port);
    }
}

IpAddr::IpAddr(const sockaddr & addr): IpAddr(addr, 0) {}

IpAddr::IpAddr(const sockaddr_in & addr): IpAddr(reinterpret_cast<const sockaddr &>(addr), sizeof(sockaddr_in)) {}

IpAddr::IpAddr(const sockaddr_in6 & addr): IpAddr(reinterpret_cast<const sockaddr &>(addr), sizeof(sockaddr_in6)) {}

IpAddr::~IpAddr() {}

void IpAddr::set_hostname(std::string_view hostname)
{
    _hostname = hostname;
}

bool IpAddr::empty() const
{
    return _addr.sockaddr.sa_family == PF_UNSPEC;
}

bool IpAddr::has_ip() const
{
    return this->is_ipv4() || this->is_ipv6();
}

bool IpAddr::is_ipv4() const
{
    return _addr.sockaddr.sa_family == AF_INET;
}

bool IpAddr::is_ipv6() const
{
    return _addr.sockaddr.sa_family == AF_INET6;
}

IpAddr IpAddr::localhost(int port, bool ipv6)
{
    return {ipv6 ? "::1" : "127.0.0.1", port};
}

bool IpAddr::set_subnet_mask(std::string_view mask)
{
    struct sockaddr_in sockaddr_mask;
    if (to_sockaddr_in(&sockaddr_mask, mask))
    {
        if (is_valid_netmask(htonl(sockaddr_mask.sin_addr.s_addr)) == false)
        {
            SIHD_LOG(error, "IpAddr: not a valid mask: {}", mask);
            return false;
        }
        _netmask_value = std::bitset<32>(sockaddr_mask.sin_addr.s_addr).count();
        return true;
    }
    return false;
}

Subnet IpAddr::subnet() const
{
    Subnet ret;

    memset(&ret, 0, sizeof(Subnet));

    if (this->has_subnet())
    {
        ret.netmask.s_addr = to_netmask(_netmask_value);
        ret.netid.s_addr = _addr.sockaddr_in.sin_addr.s_addr & ret.netmask.s_addr;
        ret.wildcard.s_addr = ~ret.netmask.s_addr;
        ret.broadcast.s_addr = ret.netid.s_addr | ret.wildcard.s_addr;
        ret.hostmin.s_addr = ntohl(htonl(ret.netid.s_addr) + 1);
        ret.hostmax.s_addr = ntohl(htonl(ret.broadcast.s_addr) - 1);
        ret.hosts = htonl(ret.wildcard.s_addr) - 1;
    }

    return ret;
}

std::string IpAddr::dump_subnet() const
{
    Subnet subnet = this->subnet();

    return fmt::format("network id: {}\nwildcard: {}\nnetmask: {}\nhost min: {}\n"
                       "host max: {}\nbroadcast: {}\nnumber of hosts: {}\n",
                       ip::to_str(&subnet.netid),
                       ip::to_str(&subnet.wildcard),
                       ip::to_str(&subnet.netmask),
                       ip::to_str(&subnet.hostmin),
                       ip::to_str(&subnet.hostmax),
                       ip::to_str(&subnet.broadcast),
                       subnet.hosts);
}

bool IpAddr::has_subnet() const
{
    return _netmask_value > 0;
}

uint32_t IpAddr::subnet_value() const
{
    return _netmask_value;
}

bool IpAddr::set_subnet_mask(uint32_t mask_value)
{
    uint32_t actual_mask = to_netmask(mask_value);
    if (is_valid_netmask(htonl(actual_mask)) == false)
    {
        SIHD_LOG(error, "IpAddr: not a valid mask: {}", mask_value);
        return false;
    }
    _netmask_value = mask_value;
    return true;
}

bool IpAddr::is_same_subnet(const IpAddr & other_addr) const
{
    return this->is_same_subnet(other_addr.addr());
}

bool IpAddr::is_same_subnet(const sockaddr & other_addr) const
{
    if (other_addr.sa_family == AF_INET && this->is_ipv4())
        return this->is_same_subnet(reinterpret_cast<const sockaddr_in &>(other_addr));
    else if (other_addr.sa_family == AF_INET6 && this->is_ipv6())
        return this->is_same_subnet(reinterpret_cast<const sockaddr_in6 &>(other_addr));
    return false;
}

bool IpAddr::is_same_subnet(const sockaddr_in & other_addr) const
{
    struct in_addr netmask;
    struct in_addr netid;

    netmask.s_addr = to_netmask(_netmask_value);
    netid.s_addr = _addr.sockaddr_in.sin_addr.s_addr & netmask.s_addr;

    return (other_addr.sin_addr.s_addr & netmask.s_addr) == (netid.s_addr & netmask.s_addr);
}

bool IpAddr::is_same_subnet(const sockaddr_in6 & addr) const
{
#pragma message("TODO handle ipv6 same subnet")
    (void)addr;
    return false;
}

const struct sockaddr & IpAddr::addr() const
{
    return _addr.sockaddr;
}

const struct sockaddr_in & IpAddr::addr4() const
{
    return _addr.sockaddr_in;
}

const struct sockaddr_in6 & IpAddr::addr6() const
{
    return _addr.sockaddr_in6;
}

std::string IpAddr::str() const
{
    if (this->is_ipv4())
        return ip::to_str(&_addr.sockaddr_in.sin_addr);
    else if (this->is_ipv6())
        return ip::to_str(&_addr.sockaddr_in6.sin6_addr);
    return "";
}

const std::string & IpAddr::hostname() const
{
    return _hostname;
}

std::string IpAddr::fetch_name() const
{
    if (!this->has_ip())
        throw std::invalid_argument("cannot fetch name of IpAddr with no ip");

    const size_t addr_len = this->is_ipv6() ? sizeof(struct sockaddr_in6) : sizeof(struct sockaddr_in);

    char host[NI_MAXHOST];
    const int flags = NI_NAMEREQD;
    const int ret = ::getnameinfo(&_addr.sockaddr, addr_len, host, NI_MAXHOST, nullptr, 0, flags);
    if (ret != 0)
    {
        SIHD_LOG(error, "getnameinfo error: {}", gai_strerror(ret));
        return "";
    }
    return host;
}

size_t IpAddr::addr_len() const
{
    if (this->is_ipv4())
        return sizeof(struct sockaddr_in);
    else if (this->is_ipv6())
        return sizeof(struct sockaddr_in6);
    return 0;
}

uint16_t IpAddr::port() const
{
    if (this->is_ipv4())
        return _addr.sockaddr_in.sin_port;
    else if (this->is_ipv6())
        return _addr.sockaddr_in6.sin6_port;
    return 0;
}

} // namespace sihd::net
