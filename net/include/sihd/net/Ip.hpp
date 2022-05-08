#ifndef __SIHD_NET_IP_HPP__
# define __SIHD_NET_IP_HPP__

# include <sihd/net/NetUtils.hpp>

namespace sihd::net
{

class Ip
{
    public:
        static const char *domain_str(int domain);
        static const char *socktype_str(int socktype);
        static const char *protocol_str(int protocol);
        static int protocol(std::string_view name);
        static int domain(std::string_view name);
        static int socktype(std::string_view name);
};



}

#endif