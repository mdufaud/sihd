#ifndef __SIHD_NET_NETINTERFACES_HPP__
# define __SIHD_NET_NETINTERFACES_HPP__

# include <sihd/net/If.hpp>

namespace sihd::net
{

class NetIFace
{
    public:
        NetIFace() {};
        ~NetIFace() {};

        const std::string & name() const { return _name; }
        unsigned int flags() const { return _flags; }

# if !defined(__SIHD_WINDOWS__)
        void add(struct ifaddrs *addr);

        const struct ifaddrs *get_addr(int family) const;

        const struct ifaddrs *find(sockaddr_in addr_in) const;
        const struct ifaddrs *find(sockaddr_in6 addr_in6) const;

        bool get_netmask(const struct ifaddrs *addr, in_addr *ret) const;
        bool get_netmask(const struct ifaddrs *addr, in6_addr *ret) const;

        const std::vector<struct ifaddrs *> & addresses() const { return _addrs; }

        //Interface is running.
        bool up() const { return _flags & IFF_UP; }
        //Valid broadcast address set.
        bool broadcast() const { return _flags & IFF_BROADCAST; }
        bool loopback() const { return _flags & IFF_LOOPBACK; }
        //Interface is a point-to-point link.
        bool point2point() const { return _flags & IFF_POINTOPOINT; }
        //Resources allocated.
        bool running() const { return _flags & IFF_RUNNING; }
        //No arp protocol, L2 destination address not set.
        bool noarp() const { return _flags & IFF_NOARP; }
        //Interface is in promiscuous mode.
        bool promisc() const { return _flags & IFF_PROMISC; }
        //Avoid use of trailers
        bool notrailers() const { return _flags & IFF_NOTRAILERS; }
        //Master of a load balancing bundle.
        bool master() const { return _flags & IFF_MASTER; }
        //Slave of a load balancing bundle.
        bool slave() const { return _flags & IFF_SLAVE; }
        //Receive all multicast packets.
        bool all_multicast() const { return _flags & IFF_ALLMULTI; }
        //Supports multicast
        bool supports_multicast() const { return _flags & IFF_MULTICAST; }
# endif

    private:
        std::string _name;
        unsigned int _flags;
# if !defined(__SIHD_WINDOWS__)
        std::vector<struct ifaddrs *> _addrs;
# endif
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

# if !defined(__SIHD_WINDOWS__)
        struct ifaddrs *ifaddrs() { return _addrs_ptr; }
# endif

    protected:

    private:
        void _free();
        bool _error;
        std::map<std::string, NetIFace> _ifaces_map;

# if !defined(__SIHD_WINDOWS__)
        struct ifaddrs *_addrs_ptr;
# endif

};

}

#endif