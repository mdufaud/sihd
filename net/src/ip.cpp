#include <strings.h>

#include <cerrno>
#include <cstring>
#include <map>

#include <sihd/net/ip.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/platform.hpp>
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
    static std::map<std::string_view, int> domain_to_str = {
        {"unix", PF_UNIX},
        {"ipv4", PF_INET},
        {"ipv6", PF_INET6},
        /*
        {"ipx", PF_IPX}, {"netlink", PF_NETLINK}, {"x25", PF_X25}, {"ax25", PF_AX25},
        {"atmpvc", PF_ATMPVC}, {"appletalk", PF_APPLETALK}, {"packet", PF_PACKET},
        */
    };
    auto it = domain_to_str.find(name);
    if (it == domain_to_str.end())
        return -1;
    return it->second;
}

int socktype(std::string_view name)
{
    static std::map<std::string_view, int> socktype_to_str = {
        {"udp", SOCK_DGRAM},
        {"tcp", SOCK_STREAM},
        {"datagram", SOCK_DGRAM},
        {"stream", SOCK_STREAM},
        {"raw", SOCK_RAW},
        {"seqpacket", SOCK_SEQPACKET},
        {"rdm", SOCK_RDM},
        {"packet", SOCK_PACKET},
    };
    auto it = socktype_to_str.find(name);
    if (it == socktype_to_str.end())
        return -1;
    return it->second;
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

uint32_t to_netmask(uint32_t value)
{
    return ntohl(0xffffffff << (32 - value));
}

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

} // namespace sihd::net::ip
