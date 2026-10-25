
#include <strings.h>

#include <bitset>
#include <cstring>
#include <stdexcept>

#include <fmt/format.h>

#include <sihd/net/IpAddr.hpp>
#include <sihd/net/ip.hpp>
#include <sihd/sys/os.hpp>
#include <sihd/sys/platform.hpp>
#include <sihd/util/Result.hpp>
#include <sihd/util/str.hpp>

#if !defined(__SIHD_WINDOWS__)
# include <arpa/inet.h>
# include <netdb.h>
# include <netinet/in.h>
# include <sys/socket.h>
#else
# include <winsock2.h>

# include <ws2tcpip.h>
#endif

namespace sihd::net
{

using namespace sihd::util;
using enum ErrorCode;
using namespace sihd::net;

namespace
{

Result<uint32_t> netmask_value_from_str(std::string_view mask_value_str)
{
    if (str::is_number(mask_value_str))
    {
        if (const auto mask_value = str::convert_from_string<uint32_t>(mask_value_str))
            return *mask_value;
    }
    return std::unexpected(Error(invalid_argument, "not a subnet mask: {}", mask_value_str));
}

Result<uint32_t> netmask_value_for_family(uint32_t mask_value, int family)
{
    const uint32_t max_value = (family == AF_INET6) ? 128 : 32;
    if (mask_value > max_value)
        return std::unexpected(Error(invalid_argument, "subnet mask out of range: {}", mask_value));
    return mask_value;
}

Result<void> to_sockaddr_in(sockaddr_in *addr, const std::string & ip, int port = 0)
{
    int ret = inet_pton(AF_INET, ip.c_str(), &addr->sin_addr);
    if (ret == 0)
        return std::unexpected(Error(invalid_argument, "not an ipv4 address: {}", ip));
    if (ret < 0)
        return std::unexpected(Error(io_error, "ipv4 address '{}': {}", ip, sihd::sys::os::last_socket_error_str()));
    addr->sin_family = AF_INET;
    addr->sin_port = htons(port);
    return {};
}

Result<void> to_sockaddr_in6(sockaddr_in6 *addr, const std::string & ip, int port = 0)
{
    int ret = inet_pton(AF_INET6, ip.c_str(), &addr->sin6_addr);
    if (ret == 0)
        return std::unexpected(Error(invalid_argument, "not an ipv6 address: {}", ip));
    if (ret < 0)
        return std::unexpected(Error(io_error, "ipv6 address '{}': {}", ip, sihd::sys::os::last_socket_error_str()));
    addr->sin6_family = AF_INET6;
    addr->sin6_port = htons(port);
    return {};
}

} // namespace

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
    uint32_t mask_value = 0;
    const size_t idx = host.find('/');
    if (idx != std::string::npos)
    {
        ip = host.substr(0, idx);
        mask_value = netmask_value_from_str(host.substr(idx + 1)).value_or(0);
    }
    else
    {
        ip = host;
    }

    if (to_sockaddr_in(&_addr.sockaddr_in, ip, port))
        _netmask_value = netmask_value_for_family(mask_value, AF_INET).value_or(0);
    else if (to_sockaddr_in6(&_addr.sockaddr_in6, ip, port))
        _netmask_value = netmask_value_for_family(mask_value, AF_INET6).value_or(0);
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
        memcpy(&_addr.sockaddr, &addr, sizeof(sockaddr_in6));
        _port = ntohs(_addr.sockaddr_in6.sin6_port);
    }
}

IpAddr::IpAddr(const sockaddr & addr): IpAddr(addr, 0) {}

IpAddr::IpAddr(const sockaddr_in & addr): IpAddr(reinterpret_cast<const sockaddr &>(addr), sizeof(sockaddr_in)) {}

IpAddr::IpAddr(const sockaddr_in6 & addr): IpAddr(reinterpret_cast<const sockaddr &>(addr), sizeof(sockaddr_in6)) {}

IpAddr::IpAddr(const IpAddr & addr): IpAddr()
{
    *this = addr;
}

IpAddr & IpAddr::operator=(const IpAddr & addr)
{
    if (this == &addr)
        return *this;
    this->_hostname = addr._hostname;
    this->_port = addr._port;
    memcpy(&this->_addr.sockaddr, &addr._addr.sockaddr, sizeof(struct sockaddr_in6));
    this->_netmask_value = addr._netmask_value;
    return *this;
}

bool IpAddr::operator==(const IpAddr & other) const
{
    if (_addr.sockaddr.sa_family != other._addr.sockaddr.sa_family)
        return false;
    if (this->port() != other.port())
        return false;
    if (_addr.sockaddr.sa_family == AF_INET)
        return memcmp(&_addr.sockaddr_in.sin_addr, &other._addr.sockaddr_in.sin_addr, sizeof(in_addr)) == 0;
    if (_addr.sockaddr.sa_family == AF_INET6)
        return memcmp(&_addr.sockaddr_in6.sin6_addr, &other._addr.sockaddr_in6.sin6_addr, sizeof(in6_addr)) == 0;
    return true;
}

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

IpAddr IpAddr::from_bytes(int family, std::span<const uint8_t> bytes)
{
    if (family == AF_INET && bytes.size() == sizeof(in_addr))
    {
        sockaddr_in addr {};
        addr.sin_family = AF_INET;
        memcpy(&addr.sin_addr, bytes.data(), sizeof(in_addr));
        return IpAddr(addr);
    }
    if (family == AF_INET6 && bytes.size() == sizeof(in6_addr))
    {
        sockaddr_in6 addr {};
        addr.sin6_family = AF_INET6;
        memcpy(&addr.sin6_addr, bytes.data(), sizeof(in6_addr));
        return IpAddr(addr);
    }
    return {};
}

std::vector<uint8_t> IpAddr::bytes() const
{
    std::vector<uint8_t> bytes;
    if (this->is_ipv4())
    {
        bytes.resize(sizeof(in_addr));
        memcpy(bytes.data(), &_addr.sockaddr_in.sin_addr, sizeof(in_addr));
    }
    else if (this->is_ipv6())
    {
        bytes.resize(sizeof(in6_addr));
        memcpy(bytes.data(), &_addr.sockaddr_in6.sin6_addr, sizeof(in6_addr));
    }
    return bytes;
}

Result<void> IpAddr::set_subnet_mask(const std::string & mask)
{
    struct sockaddr_in sockaddr_mask;
    if (to_sockaddr_in(&sockaddr_mask, mask).has_value() == false)
        return std::unexpected(Error(invalid_argument, "cannot parse mask '{}'", mask));
    if (ip::is_valid_netmask(htonl(sockaddr_mask.sin_addr.s_addr)) == false)
        return std::unexpected(Error(invalid_argument, "not a valid mask: {}", mask));
    _netmask_value = std::bitset<32>(sockaddr_mask.sin_addr.s_addr).count();
    return {};
}

Subnet IpAddr::subnet() const
{
    Subnet ret;

    memset(&ret, 0, sizeof(Subnet));

    if (this->is_ipv4() == false || this->has_subnet() == false)
        return ret;

    ret.netmask.s_addr = ip::to_netmask(_netmask_value);
    ret.netid.s_addr = _addr.sockaddr_in.sin_addr.s_addr & ret.netmask.s_addr;
    ret.wildcard.s_addr = ~ret.netmask.s_addr;
    ret.broadcast.s_addr = ret.netid.s_addr | ret.wildcard.s_addr;
    ret.hostmin.s_addr = ntohl(htonl(ret.netid.s_addr) + 1);
    ret.hostmax.s_addr = ntohl(htonl(ret.broadcast.s_addr) - 1);
    ret.hosts = htonl(ret.wildcard.s_addr);
    if (ret.hosts != 0)
        ret.hosts -= 1;

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
    const int family = this->addr().sa_family;
    if (family != AF_INET && family != AF_INET6)
        return false;
    const auto validated = netmask_value_for_family(mask_value, family);
    if (validated.has_value() == false)
        return false;
    _netmask_value = *validated;
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
    if (this->is_ipv4() == false || !this->has_subnet())
        return false;
    struct in_addr netmask;
    struct in_addr netid;

    netmask.s_addr = ip::to_netmask(_netmask_value);
    netid.s_addr = _addr.sockaddr_in.sin_addr.s_addr & netmask.s_addr;

    return (other_addr.sin_addr.s_addr & netmask.s_addr) == (netid.s_addr & netmask.s_addr);
}

bool IpAddr::is_same_subnet(const sockaddr_in6 & addr) const
{
    if (this->is_ipv6() == false || !this->has_subnet())
        return false;
    struct in6_addr mask;
    struct in6_addr masked_addr1;
    struct in6_addr masked_addr2;

    memset(&mask, 0, sizeof(mask));
    for (uint32_t i = 0; i < _netmask_value / 8; i++)
    {
        mask.s6_addr[i] = 0xFF;
    }
    if (_netmask_value % 8 != 0)
    {
        mask.s6_addr[_netmask_value / 8] = (uint8_t)(0xFF << (8 - (_netmask_value % 8)));
    }

    for (int i = 0; i < 16; i++)
    {
        masked_addr1.s6_addr[i] = _addr.sockaddr_in6.sin6_addr.s6_addr[i] & mask.s6_addr[i];
        masked_addr2.s6_addr[i] = addr.sin6_addr.s6_addr[i] & mask.s6_addr[i];
    }

    return memcmp(&masked_addr1, &masked_addr2, sizeof(masked_addr1)) == 0;
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

Result<void> IpAddr::fetch_hostname()
{
    if (!this->has_ip())
        return std::unexpected(Error(ErrorCode::invalid_argument, "cannot fetch name of IpAddr with no ip"));

    const size_t addr_len = this->is_ipv6() ? sizeof(struct sockaddr_in6) : sizeof(struct sockaddr_in);

    char hostname[NI_MAXHOST];
    const int flags = NI_NAMEREQD | NI_NOFQDN;
    const int ret = ::getnameinfo(&_addr.sockaddr, addr_len, hostname, NI_MAXHOST, nullptr, 0, flags);
    if (ret != 0)
        return std::unexpected(Error(not_found, "getnameinfo error: {}", gai_strerror(ret)));

    this->set_hostname(hostname);
    return {};
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
        return ntohs(_addr.sockaddr_in.sin_port);
    else if (this->is_ipv6())
        return ntohs(_addr.sockaddr_in6.sin6_port);
    return 0;
}

} // namespace sihd::net
