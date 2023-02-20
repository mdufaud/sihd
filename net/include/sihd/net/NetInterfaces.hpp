#ifndef __SIHD_NET_NETINTERFACES_HPP__
#define __SIHD_NET_NETINTERFACES_HPP__

#include <map>

#include <sihd/net/utils.hpp>

#if !defined(__SIHD_WINDOWS__)
# include <netinet/in.h> // sockaddr
#endif

struct ifaddrs;

namespace sihd::net
{

class NetIFace
{
    public:
        NetIFace() {};
        ~NetIFace() {};

        const std::string & name() const { return _name; }
        unsigned int flags() const { return _flags; }

#if !defined(__SIHD_WINDOWS__)
        void add(struct ifaddrs *addr);

        const struct ifaddrs *get_addr(int family) const;

        const struct ifaddrs *find(sockaddr_in addr_in) const;
        const struct ifaddrs *find(sockaddr_in6 addr_in6) const;

        bool get_netmask(const struct ifaddrs *addr, in_addr *ret) const;
        bool get_netmask(const struct ifaddrs *addr, in6_addr *ret) const;

        const std::vector<struct ifaddrs *> & addresses() const { return _addrs; }

        // Interface is running.
        bool up() const;
        // Valid broadcast address set.
        bool broadcast() const;
        bool loopback() const;
        // Interface is a point-to-point link.
        bool point2point() const;
        // Resources allocated.
        bool running() const;
        // No arp protocol, L2 destination address not set.
        bool noarp() const;
        // Interface is in promiscuous mode.
        bool promisc() const;
        // Avoid use of trailers
        bool notrailers() const;
        // Master of a load balancing bundle.
        bool master() const;
        // Slave of a load balancing bundle.
        bool slave() const;
        // Receive all multicast packets.
        bool all_multicast() const;
        // Supports multicast
        bool supports_multicast() const;
#endif

    private:
        std::string _name;
        unsigned int _flags;
#if !defined(__SIHD_WINDOWS__)
        std::vector<struct ifaddrs *> _addrs;
#endif
};

class NetInterfaces
{
    public:
        NetInterfaces();
        virtual ~NetInterfaces();

        bool load();
        bool error();

        std::vector<std::string> names();
        std::vector<NetIFace *> ifaces();

#if !defined(__SIHD_WINDOWS__)
        struct ifaddrs *ifaddrs() { return _addrs_ptr; }
#endif

    protected:

    private:
        void _free();
        bool _error;
        std::map<std::string, NetIFace> _ifaces_map;

#if !defined(__SIHD_WINDOWS__)
        struct ifaddrs *_addrs_ptr;
#endif
};

} // namespace sihd::net

#endif
