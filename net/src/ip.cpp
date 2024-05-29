#include <strings.h>

#include <cerrno>
#include <cstring>
#include <map>

#include <sihd/util/Logger.hpp>

#include <sihd/net/ip.hpp>
#include <sihd/util/platform.hpp>

#if !defined(__SIHD_WINDOWS__)
# include <netdb.h>      // getprotobynumber, getprotobyname
# include <sys/socket.h> // PF_UNIX...
#else
# include <sihd/net/utils.hpp>
#endif

namespace sihd::net::ip
{

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

} // namespace sihd::net::ip
