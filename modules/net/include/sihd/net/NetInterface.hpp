#ifndef __SIHD_NET_NETINTERFACE_HPP__
#define __SIHD_NET_NETINTERFACE_HPP__

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>

#include <sihd/net/IpAddr.hpp>
#include <sihd/util/Result.hpp>

namespace sihd::net
{

class NetInterface
{
    public:
        NetInterface();
        ~NetInterface();

        static sihd::util::Result<std::map<std::string, NetInterface>> get_all_interfaces();

        const std::string & name() const;
        const std::string & mac_addr() const;

        const IpAddr & addr4() const;
        const IpAddr & netmask4() const;
        // either point-to-point link or broadcast
        const IpAddr & extra_addr4() const;

        const IpAddr & addr6() const;
        const IpAddr & netmask6() const;
        // either point-to-point link or broadcast
        const IpAddr & extra_addr6() const;

        bool up() const;
        bool broadcast() const;
        bool loopback() const;
        bool point2point() const;
        bool running() const;
        bool noarp() const;
        bool promisc() const;
        bool notrailers() const;
        bool master() const;
        bool slave() const;
        bool all_multicast() const;
        bool supports_multicast() const;

    protected:
        void set_name(std::string_view name);
        void set_flags(uint32_t flags);
        void set_ipv4(IpAddr && addr, IpAddr && netmask, IpAddr && extra);
        void set_ipv6(IpAddr && addr, IpAddr && netmask, IpAddr && extra);
        void set_macaddr(std::string_view mac_addr);

    private:
        std::string _name;
        uint32_t _flags = 0;

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
