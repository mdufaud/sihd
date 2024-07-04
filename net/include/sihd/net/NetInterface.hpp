#ifndef __SIHD_NET_NETINTERFACE_HPP__
#define __SIHD_NET_NETINTERFACE_HPP__

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>

#include <sihd/net/IpAddr.hpp>

namespace sihd::net
{

class NetInterface
{
    public:
        NetInterface();
        ~NetInterface();

        static std::optional<std::map<std::string, NetInterface>> get_all_interfaces();

        const std::string & name() const { return _name; }
        const std::string & mac_addr() const { return _mac_address; }

        const IpAddr & addr4() const { return _addr4; }
        const IpAddr & netmask4() const { return _netmask4; }
        // either point-to-point link or broadcast
        const IpAddr & extra_addr4() const { return _extra_addr4; }

        const IpAddr & addr6() const { return _addr6; }
        const IpAddr & netmask6() const { return _netmask6; }
        // either point-to-point link or broadcast
        const IpAddr & extra_addr6() const { return _extra_addr6; }

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

    protected:
        void set_name(std::string_view name);
        void set_flags(uint32_t flags);
        void set_ipv4(IpAddr && addr, IpAddr && netmask, IpAddr && extra);
        void set_ipv6(IpAddr && addr, IpAddr && netmask, IpAddr && extra);
        void set_macaddr(std::string_view mac_addr);

    private:
        std::string _name;
        uint32_t _flags;

        IpAddr _addr4;
        IpAddr _netmask4;
        IpAddr _extra_addr4;

        IpAddr _addr6;
        IpAddr _netmask6;
        IpAddr _extra_addr6;

        std::string _mac_address;
};

} // namespace sihd::net

#endif
