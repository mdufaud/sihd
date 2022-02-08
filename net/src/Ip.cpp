#include <sihd/net/Ip.hpp>
#include <sihd/util/Logger.hpp>

#include <strings.h>
#include <string.h>
#include <errno.h>

#if !defined(__SIHD_WINDOWS__)
#endif

namespace sihd::net
{

SIHD_NEW_LOGGER("sihd::net");

std::map<std::string, int> Ip::_domain_to_str = {
    {"unix", PF_UNIX}, {"ipv4", PF_INET}, {"ipv6", PF_INET6},
    /*
    {"ipx", PF_IPX}, {"netlink", PF_NETLINK}, {"x25", PF_X25}, {"ax25", PF_AX25},
    {"atmpvc", PF_ATMPVC}, {"appletalk", PF_APPLETALK}, {"packet", PF_PACKET},
    */
};

std::map<std::string, int> Ip::_socktype_to_str = {
    {"udp", SOCK_DGRAM}, {"tcp", SOCK_STREAM},
    {"datagram", SOCK_DGRAM}, {"stream", SOCK_STREAM}, {"raw", SOCK_RAW},
    {"seqpacket", SOCK_SEQPACKET}, {"rdm", SOCK_RDM}, {"packet", SOCK_PACKET},
};

std::string     Ip::domain_to_string(int domain)
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

std::string     Ip::socktype_to_string(int socktype)
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

std::string     Ip::protocol_to_string(int protocol)
{
    struct protoent *pe = getprotobynumber(protocol);
    if (pe == nullptr)
        return "";
    return pe->p_name;
}

int     Ip::protocol(const std::string & name)
{
    struct protoent *pe = getprotobyname(name.c_str());
    if (pe == nullptr)
        return -1;
    return pe->p_proto;
}

int     Ip::domain(const std::string & name)
{
    if (_domain_to_str.find(name) == _domain_to_str.end())
        return -1;
    return _domain_to_str[name];
}

int     Ip::socktype(const std::string & name)
{
    if (_socktype_to_str.find(name) == _socktype_to_str.end())
        return -1;
    return _socktype_to_str[name];
}

}