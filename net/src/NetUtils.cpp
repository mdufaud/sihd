#include <string.h>

#include <sihd/net/NetUtils.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/OS.hpp>

#if !defined(__SIHD_WINDOWS__)
# include <net/if.h>
# include <sys/ioctl.h>
#endif

namespace sihd::net
{

SIHD_LOGGER;

//Source code from the book "UNIX Network Programming"
uint16_t    NetUtils::checksum(uint16_t *addr, int len)
{
	int nleft = len;
	uint32_t sum = 0;
	uint16_t *w = addr;
	uint16_t answer = 0;

	/*
	* Our algorithm is simple, using a 32 bit accumulator (sum), we add
	* sequential 16 bit words to it, and at the end, fold back all the
	* carry bits from the top 16 bits into the lower 16 bits.
	*/
	while (nleft > 1)
	{
		sum += *w++;
		nleft -= 2;
	}

	/* 4mop ​​up an odd byte, if necessary */
	if (nleft == 1)
    {
		*(unsigned char *)(&answer) = *(unsigned char *)w;
		sum += answer;
	}

	/* 4add back carry outs from top 16 bits to low 16 bits */
	sum = (sum >> 16) + (sum & 0xffff);/* add hi 16 to low 16 */
	sum += (sum >> 16);/* add carry */
	answer = ~sum;/* truncate to 16 bits */
	return answer;
}

#if !defined(__SIHD_WINDOWS__)

int     NetUtils::interface_idx(int sock, std::string_view name)
{
    struct ifreq iface;
    memset(&iface, 0, sizeof(struct ifreq));
    strncpy(iface.ifr_name, name.data(), IFNAMSIZ - 1);
    if (sihd::util::OS::ioctl(sock, SIOCGIFINDEX, &iface, true) != 0)
        return -1;
    return iface.ifr_ifindex;
}

bool    NetUtils::get_interface_name(int sock, int idx, std::string & to_fill)
{
    struct ifreq iface;
    memset(&iface, 0, sizeof(struct ifreq));
    iface.ifr_ifindex = idx;
    if (sihd::util::OS::ioctl(sock, SIOCGIFNAME, &iface, true) != 0)
        return false;
    to_fill = iface.ifr_name;
    return true;
}

bool    NetUtils::get_interface_mac(int sock, std::string_view name, struct sockaddr *to_fill)
{
    struct ifreq iface;
    memset(&iface, 0, sizeof(struct ifreq));
    iface.ifr_addr.sa_family = to_fill->sa_family;
    strncpy(iface.ifr_name, name.data(), IFNAMSIZ - 1);
    if (sihd::util::OS::ioctl(sock, SIOCGIFHWADDR, &iface, true) != 0)
        return false;
    memcpy(to_fill, &iface.ifr_hwaddr, sizeof(struct sockaddr));
    return true;
}

bool    NetUtils::get_interface_addr(int sock, std::string_view name, struct sockaddr *to_fill)
{
    struct ifreq iface;
    memset(&iface, 0, sizeof(struct ifreq));
    iface.ifr_addr.sa_family = to_fill->sa_family;
    strncpy(iface.ifr_name, name.data(), IFNAMSIZ - 1);
    if (sihd::util::OS::ioctl(sock, SIOCGIFADDR, &iface, true) != 0)
        return false;
    memcpy(to_fill, &iface.ifr_addr, sizeof(struct sockaddr));
    return true;
}

bool    NetUtils::get_interface_broadcast(int sock, std::string_view name, struct sockaddr *to_fill)
{
    struct ifreq iface;
    memset(&iface, 0, sizeof(struct ifreq));
    iface.ifr_addr.sa_family = to_fill->sa_family;
    strncpy(iface.ifr_name, name.data(), IFNAMSIZ - 1);
    if (sihd::util::OS::ioctl(sock, SIOCGIFBRDADDR, &iface, true) != 0)
        return false;
    memcpy(to_fill, &iface.ifr_broadaddr, sizeof(struct sockaddr));
    return true;
}

bool    NetUtils::get_interface_netmask(int sock, std::string_view name, struct sockaddr *to_fill)
{
    struct ifreq iface;
    memset(&iface, 0, sizeof(struct ifreq));
    iface.ifr_addr.sa_family = to_fill->sa_family;
    strncpy(iface.ifr_name, name.data(), IFNAMSIZ - 1);
    if (sihd::util::OS::ioctl(sock, SIOCGIFNETMASK, &iface, true) != 0)
        return false;
    memcpy(to_fill, &iface.ifr_netmask, sizeof(struct sockaddr));
    return true;
}

#else

int     NetUtils::interface_idx(int sock, std::string_view name)
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