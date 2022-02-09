#ifndef __SIHD_NET_NETUTILS_HPP__
# define __SIHD_NET_NETUTILS_HPP__

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
#  include <net/if.h>
#  include <sys/un.h>
#  include <netinet/tcp.h>
#  include <fcntl.h>
#  include <ifaddrs.h>
# else
// shutdown function corresponding values unix -> windows
#  define SHUT_RD SD_RECEIVE
#  define SHUT_WR SD_SEND
#  define SHUT_RDWR SD_BOTH
// missing mingw getsockopt action
#  ifndef SO_BSP_STATE
#   define SO_BSP_STATE 0x1009
#  endif
// missing socket types
#  define SOCK_SEQPACKET 5
#  define SOCK_PACKET 10
#  include <winsock2.h>
#  include <ws2def.h>
#  include <winsock.h>
#  include <ws2tcpip.h>
# endif

namespace sihd::net
{

class NetUtils
{
    public:
        static int get_interface_idx(int sock, const std::string & name);
        static bool get_interface_name(int sock, int idx, std::string & to_fill);
        static bool get_interface_mac(int sock, const std::string & name, struct sockaddr *to_fill);
        static bool get_interface_addr(int sock, const std::string & name, struct sockaddr *to_fill);
        static bool get_interface_broadcast(int sock, const std::string & name, struct sockaddr *to_fill);
        static bool get_interface_netmask(int sock, const std::string & name, struct sockaddr *to_fill);

    protected:

    private:
        NetUtils() {};
        ~NetUtils() {};
};

}

#endif