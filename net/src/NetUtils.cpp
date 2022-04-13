#include <sihd/net/NetUtils.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/OS.hpp>
#include <string.h>

#if !defined(__SIHD_WINDOWS__)
# include <net/if.h>
#endif

namespace sihd::net
{

SIHD_LOGGER;

#if !defined(__SIHD_WINDOWS__)

int     NetUtils::get_interface_idx(int sock, std::string_view name)
{
    struct ifreq iface;
    memset(&iface, 0, sizeof(struct ifreq));
    strncpy(iface.ifr_name, name.data(), IFNAMSIZ - 1);
    if (sihd::util::OS::ioctl(sock, SIOCGIFINDEX, &iface) == 0)
        return iface.ifr_ifindex;
    return -1;
}

bool    NetUtils::get_interface_name(int sock, int idx, std::string & to_fill)
{
    struct ifreq iface;
    memset(&iface, 0, sizeof(struct ifreq));
    iface.ifr_ifindex = idx;
    if (sihd::util::OS::ioctl(sock, SIOCGIFNAME, &iface) != 0)
        return false;
    to_fill = iface.ifr_name;
    return true;
}

bool    NetUtils::get_interface_mac(int sock, std::string_view name, struct sockaddr *to_fill)
{
    struct ifreq iface;
    memset(&iface, 0, sizeof(struct ifreq));
    strncpy(iface.ifr_name, name.data(), IFNAMSIZ - 1);
    if (sihd::util::OS::ioctl(sock, SIOCGIFHWADDR, &iface) != 0)
        return false;
    memcpy(to_fill, &iface.ifr_hwaddr, sizeof(struct sockaddr));
    return true;
}

bool    NetUtils::get_interface_addr(int sock, std::string_view name, struct sockaddr *to_fill)
{
    struct ifreq iface;
    memset(&iface, 0, sizeof(struct ifreq));
    strncpy(iface.ifr_name, name.data(), IFNAMSIZ - 1);
    if (sihd::util::OS::ioctl(sock, SIOCGIFADDR, &iface) != 0)
        return false;
    memcpy(to_fill, &iface.ifr_addr, sizeof(struct sockaddr));
    return true;
}

bool    NetUtils::get_interface_broadcast(int sock, std::string_view name, struct sockaddr *to_fill)
{
    struct ifreq iface;
    memset(&iface, 0, sizeof(struct ifreq));
    strncpy(iface.ifr_name, name.data(), IFNAMSIZ - 1);
    if (sihd::util::OS::ioctl(sock, SIOCGIFBRDADDR, &iface) != 0)
        return false;
    memcpy(to_fill, &iface.ifr_broadaddr, sizeof(struct sockaddr));
    return true;
}

bool    NetUtils::get_interface_netmask(int sock, std::string_view name, struct sockaddr *to_fill)
{
    struct ifreq iface;
    memset(&iface, 0, sizeof(struct ifreq));
    strncpy(iface.ifr_name, name.data(), IFNAMSIZ - 1);
    if (sihd::util::OS::ioctl(sock, SIOCGIFNETMASK, &iface) != 0)
        return false;
    memcpy(to_fill, &iface.ifr_netmask, sizeof(struct sockaddr));
    return true;
}

#else

int     NetUtils::get_interface_idx(int sock, std::string_view name)
{
    (void)sock;(void)name; return -1;
}

bool    NetUtils::get_interface_name(int sock, int idx, std::string & to_fill)
{
    (void)sock;(void)idx;(void)to_fill; return false;
}

bool    NetUtils::get_interface_mac(int sock, std::string_view name, struct sockaddr *to_fill)
{
    (void)sock;(void)name;(void)to_fill; return false;
}

bool    NetUtils::get_interface_addr(int sock, std::string_view name, struct sockaddr *to_fill)
{
    (void)sock;(void)name;(void)to_fill; return false;
}

bool    NetUtils::get_interface_broadcast(int sock, std::string_view name, struct sockaddr *to_fill)
{
    (void)sock;(void)name;(void)to_fill; return false;
}

bool    NetUtils::get_interface_netmask(int sock, std::string_view name, struct sockaddr *to_fill)
{
    (void)sock;(void)name;(void)to_fill; return false;
}

#endif


}