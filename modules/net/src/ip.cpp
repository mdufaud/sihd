#include <strings.h>

#include <cstring>

#include <sihd/net/ip.hpp>
#include <sihd/sys/platform.hpp>
#include <sihd/util/Logger.hpp>
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

SIHD_NEW_LOGGER("sihd::net::ip");

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

int protocol(std::string_view name)
{
    struct protoent *pe = getprotobyname(name.data());
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

bool is_valid_ipv4(std::string_view ip)
{
    struct sockaddr_in sa;
    return inet_pton(AF_INET, ip.data(), &(sa.sin_addr)) == 1;
}

bool is_valid_ipv6(std::string_view ip)
{
    struct sockaddr_in6 sa;
    return inet_pton(AF_INET6, ip.data(), &(sa.sin6_addr)) == 1;
}

bool is_valid_ip(std::string_view ip)
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
           || ((addr >> 16) & 0xFFFF) == 0xA9FE // 169.254.0.0/16 (link-local)
           || (addr >> 20) == 0xAC1             // 172.16.0.0/12 (class B)
           || (addr >> 16) == 0xC0A8;           // 192.168.0.0/16 (class C)
}

bool is_private_host(std::string_view host)
{
    struct addrinfo hints {};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    struct addrinfo *res = nullptr;
    if (getaddrinfo(std::string(host).c_str(), nullptr, &hints, &res) != 0)
        return false;

    bool found = false;
    for (auto *r = res; r && !found; r = r->ai_next)
    {
        if (r->ai_family == AF_INET)
        {
            uint32_t addr = ntohl(reinterpret_cast<sockaddr_in *>(r->ai_addr)->sin_addr.s_addr);
            found = is_private_ipv4(addr);
        }
        else if (r->ai_family == AF_INET6)
        {
            auto *s6 = reinterpret_cast<sockaddr_in6 *>(r->ai_addr);
            found = IN6_IS_ADDR_LOOPBACK(&s6->sin6_addr) || IN6_IS_ADDR_LINKLOCAL(&s6->sin6_addr)
                    || IN6_IS_ADDR_SITELOCAL(&s6->sin6_addr);
        }
    }
    freeaddrinfo(res);
    return found;
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
