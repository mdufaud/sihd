#ifndef __SIHD_NET_IP_HPP__
# define __SIHD_NET_IP_HPP__

# include <sihd/util/platform.hpp>
# include <vector>
# include <string>
# include <optional>
# include <map>

# if !defined(__SIHD_WINDOWS__)
#  include <sys/types.h>
#  include <sys/socket.h>
#  include <netdb.h>
#  include <arpa/inet.h>
# else
#  define SOCK_SEQPACKET 5
#  define SOCK_PACKET 10
#  include <winsock2.h>
#  include <winsock.h>
#  include <ws2tcpip.h>
# endif

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

    private:
        static std::map<std::string, int> _domain_to_str;
        static std::map<std::string, int> _socktype_to_str;

};



}

#endif