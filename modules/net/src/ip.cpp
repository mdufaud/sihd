#include <strings.h>

#include <algorithm>
#include <cstring>

#include <sihd/net/ip.hpp>
#include <sihd/sys/platform.hpp>
#include <sihd/util/endian.hpp>
#include <sihd/util/str.hpp>

#if !defined(__SIHD_WINDOWS__)
# include <arpa/inet.h>  // inet_pton...
# include <netdb.h>      // getprotobynumber, getprotobyname
# include <netinet/in.h> // struct inX_addr
#else
# include <winsock2.h>

# include <ws2tcpip.h> // addrinfo
#endif

namespace sihd::net::ip
{

using namespace sihd::util;

namespace
{

bool is_private_ipv6(const in6_addr & addr)
{
    const uint8_t *bytes = addr.s6_addr;

    if (IN6_IS_ADDR_UNSPECIFIED(&addr) || IN6_IS_ADDR_LOOPBACK(&addr) || IN6_IS_ADDR_LINKLOCAL(&addr)
        || IN6_IS_ADDR_SITELOCAL(&addr) || IN6_IS_ADDR_MULTICAST(&addr))
        return true;

    if ((bytes[0] & 0xFE) == 0xFC) // fc00::/7 unique local
        return true;

    if (IN6_IS_ADDR_V4MAPPED(&addr) || IN6_IS_ADDR_V4COMPAT(&addr)) // ::ffff:a.b.c.d and ::a.b.c.d
        return is_private_ipv4(endian::read_be<uint32_t>(bytes + 12, 4));

    if (bytes[0] == 0x00 && bytes[1] == 0x64 && bytes[2] == 0xFF && bytes[3] == 0x9B // 64:ff9b::/96 NAT64
        && std::all_of(bytes + 4, bytes + 12, [](uint8_t byte) { return byte == 0; }))
        return is_private_ipv4(endian::read_be<uint32_t>(bytes + 12, 4));

    if (bytes[0] == 0x20 && bytes[1] == 0x02) // 2002::/16 6to4
        return is_private_ipv4(endian::read_be<uint32_t>(bytes + 2, 4));

    return false;
}

} // namespace

const char *domain_str(int domain)
{
    switch (domain)
    {
        case PF_UNIX:
            return "unix";
        case PF_INET:
            return "ipv4";
        case PF_INET6:
            return "ipv6";
        /*
        case PF_IPX:
            return "ipx";
        case PF_NETLINK:
            return "netlink";
        case PF_X25:
            return "x25";
        case PF_AX25:
            return "ax25";
        case PF_ATMPVC:
            return "atmpvc";
        case PF_APPLETALK:
            return "appletalk";
        case PF_PACKET:
            return "packet";
        */
        case -1:
            return "error";
        default:
            return "";
    }
}

const char *socktype_str(int socktype)
{
    switch (socktype)
    {
        case SOCK_DGRAM:
            return "datagram";
        case SOCK_STREAM:
            return "stream";
        case SOCK_RAW:
            return "raw";
        case SOCK_SEQPACKET:
            return "seqpacket";
        case SOCK_RDM:
            return "rdm";
        case SOCK_PACKET:
            return "packet";
        case -1:
            return "error";
        default:
            return "";
    }
}

const char *protocol_str(int protocol)
{
    struct protoent *pe = getprotobynumber(protocol);
    if (pe == nullptr)
        return "";
    return pe->p_name;
}

int protocol(const std::string & name)
{
    struct protoent *pe = getprotobyname(name.c_str());
    if (pe == nullptr)
        return -1;
    return pe->p_proto;
}

int domain(std::string_view name)
{
    if (name == "unix")
        return PF_UNIX;
    if (name == "ipv4")
        return PF_INET;
    if (name == "ipv6")
        return PF_INET6;
    return -1;
}

int socktype(std::string_view name)
{
    if (name == "udp" || name == "datagram")
        return SOCK_DGRAM;
    if (name == "tcp" || name == "stream")
        return SOCK_STREAM;
    if (name == "raw")
        return SOCK_RAW;
    if (name == "seqpacket")
        return SOCK_SEQPACKET;
    if (name == "rdm")
        return SOCK_RDM;
    if (name == "packet")
        return SOCK_PACKET;
    return -1;
}

bool is_valid_ipv4(const std::string & ip)
{
    struct sockaddr_in sa;
    return inet_pton(AF_INET, ip.c_str(), &(sa.sin_addr)) == 1;
}

bool is_valid_ipv6(const std::string & ip)
{
    struct sockaddr_in6 sa;
    return inet_pton(AF_INET6, ip.c_str(), &(sa.sin6_addr)) == 1;
}

bool is_valid_ip(const std::string & ip)
{
    return is_valid_ipv4(ip) || is_valid_ipv6(ip);
}

std::string to_str(const sockaddr *addr)
{
    if (addr->sa_family == AF_INET)
        return to_str((const sockaddr_in *)addr);
    else if (addr->sa_family == AF_INET6)
        return to_str((const sockaddr_in6 *)addr);
    return "";
}

std::string to_str(const sockaddr_in *addr_in)
{
    return to_str(&addr_in->sin_addr);
}

std::string to_str(const sockaddr_in6 *addr_in)
{
    return to_str(&addr_in->sin6_addr);
}

std::string to_str(const in_addr *addr_in)
{
    char buffer[INET_ADDRSTRLEN] = {0};
    return inet_ntop(AF_INET, (const void *)(addr_in), buffer, INET_ADDRSTRLEN);
}

std::string to_str(const in6_addr *addr_in)
{
    char buffer[INET6_ADDRSTRLEN] = {0};
    return inet_ntop(AF_INET6, (const void *)(addr_in), buffer, INET6_ADDRSTRLEN);
}

bool is_private_ipv4(uint32_t addr)
{
    return (addr >> 24) == 0                    // 0.0.0.0/8 ("this network")
           || (addr >> 24) == 127               // 127.0.0.0/8 (loopback)
           || (addr >> 24) == 10                // 10.0.0.0/8 (class A)
           || (addr >> 22) == 0x191             // 100.64.0.0/10 (carrier-grade NAT)
           || ((addr >> 16) & 0xFFFF) == 0xA9FE // 169.254.0.0/16 (link-local)
           || (addr >> 20) == 0xAC1             // 172.16.0.0/12 (class B)
           || (addr >> 8) == 0xC00000           // 192.0.0.0/24 (IETF protocol assignments)
           || (addr >> 16) == 0xC0A8            // 192.168.0.0/16 (class C)
           || (addr >> 17) == 0x6309            // 198.18.0.0/15 (benchmarking)
           || (addr >> 28) >= 0xE;              // 224.0.0.0/4 multicast, 240.0.0.0/4 reserved, broadcast
}

Result<std::vector<std::string>> public_addresses(const std::string & host)
{
    struct addrinfo hints {};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    struct addrinfo *res = nullptr;
    // fail closed: a host the resolver cannot parse may still be resolved by the actual client
    if (host.empty() || host.find('\0') != std::string::npos || getaddrinfo(host.c_str(), nullptr, &hints, &res) != 0)
        return std::unexpected(Error(ErrorCode::not_found, "cannot resolve host '{}'", host));

    std::vector<std::string> addresses;
    bool found = false;
    for (auto *r = res; r && !found; r = r->ai_next)
    {
        if (r->ai_family == AF_INET)
        {
            uint32_t addr = ntohl(reinterpret_cast<sockaddr_in *>(r->ai_addr)->sin_addr.s_addr);
            found = is_private_ipv4(addr);
        }
        else if (r->ai_family == AF_INET6)
            found = is_private_ipv6(reinterpret_cast<sockaddr_in6 *>(r->ai_addr)->sin6_addr);
        else
            continue;
        addresses.push_back(to_str(r->ai_addr));
    }
    freeaddrinfo(res);
    if (addresses.empty())
        return std::unexpected(Error(ErrorCode::not_found, "host '{}' has no ip address", host));
    if (found)
        return std::unexpected(Error(ErrorCode::permission_denied, "host '{}' resolves to a private address", host));
    return addresses;
}

Result<bool> is_private_host(const std::string & host)
{
    const auto addresses = public_addresses(host);
    if (addresses.has_value())
        return false;
    if (addresses.error().code == ErrorCode::permission_denied)
        return true;
    return std::unexpected(addresses.error());
}

uint32_t to_netmask(uint32_t value)
{
    if (value == 0)
        return 0;
    if (value >= 32)
        return ntohl(0xffffffff);
    return ntohl(0xffffffff << (32 - value));
}

//  A valid netmask cannot have a zero with a one to the right of it. All zeros must have another zero to the
//  right of it or be bit 0. must apply htonl if mask is in host byte order
bool is_valid_netmask(uint32_t mask)
{
    if (mask == 0)
        return false;
    uint32_t y = ~mask;
    uint32_t z = y + 1;
    return (z & y) == 0;
}

} // namespace sihd::net::ip
