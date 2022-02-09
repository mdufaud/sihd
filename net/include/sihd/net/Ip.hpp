#ifndef __SIHD_NET_IP_HPP__
# define __SIHD_NET_IP_HPP__

# include <sihd/net/NetUtils.hpp>

namespace sihd::net
{

class Ip
{
    public:
        static std::string domain_to_string(int domain);
        static std::string socktype_to_string(int socktype);
        static std::string protocol_to_string(int protocol);
        static int protocol(const std::string & name);
        static int domain(const std::string & name);
        static int socktype(const std::string & name);
};



}

#endif